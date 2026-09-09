#include "Lazer.h"
#include "WorldTransformConfig.h"
#include <cassert>
#include <cmath>
#include <numbers>
#include <cmath>

using namespace KamataEngine;

void Lazer::Initialize(Model* model, Camera* camera, const Vector3& start, const Vector3& end, uint8_t id) {
	assert(model);
	assert(camera);

	model_ = model;
	camera_ = camera;
	id_ = id;
	isActive_ = true;
	color_.Initialize();
	color_.SetColor({1.0f, 0.1f, 0.1f, 1.0f});

	// 両端のLマスを含むレーザー全体を、プレイヤー用の障害物矩形にする。
	const float minX = start.x < end.x ? start.x : end.x;
	const float maxX = start.x > end.x ? start.x : end.x;
	const float minY = start.y < end.y ? start.y : end.y;
	const float maxY = start.y > end.y ? start.y : end.y;
	// レーザーの当たり判定の太さ
	constexpr float kCollisionThickness = 0.2f;
	const float halfThickness = kCollisionThickness / 2.0f;

	if (start.y == end.y) {
		// 横レーザー：上下だけ細くする
		collisionRect_.left = minX - MapChipField::kBlockWidth / 2.0f;
		collisionRect_.right = maxX + MapChipField::kBlockWidth / 2.0f;

		collisionRect_.bottom = start.y - halfThickness;
		collisionRect_.top = start.y + halfThickness;

	} else if (start.x == end.x) {
		// 縦レーザー：左右だけ細くする
		collisionRect_.left = start.x - halfThickness;
		collisionRect_.right = start.x + halfThickness;

		collisionRect_.bottom = minY - MapChipField::kBlockHeight / 2.0f;
		collisionRect_.top = maxY + MapChipField::kBlockHeight / 2.0f;
	}

	worldTransform_.Initialize();
	
	// Lazer.objはY軸方向の円柱で、原点がモデル中央ではない
	constexpr float kModelLength = 8.0f;
	constexpr float kModelCenterY = 4.0f;
	constexpr float kRadiusScale = 0.1f;

	Vector3 center = {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f, 0.0f};

	if (start.y == end.y) {
		// 横レーザー
		// 両端のLマス全体まで届くように1マス分を加える
		float length = std::abs(end.x - start.x) + 1.0f;
		float lengthScale = length / kModelLength;
		worldTransform_.scale_ = {kRadiusScale, lengthScale, kRadiusScale};
		worldTransform_.rotation_.z = std::numbers::pi_v<float> / 2.0f;
		// +Y軸を+90度回転すると-X軸方向になるため、モデル中心のずれを補正する
		worldTransform_.translation_ = {center.x + kModelCenterY * lengthScale, center.y, center.z};

	} else if (start.x == end.x) {
		// 縦レーザー
		// 両端のLマス全体まで届くように1マス分を加える
		float length = std::abs(end.y - start.y) + 1.0f;
		float lengthScale = length / kModelLength;
		worldTransform_.scale_ = {kRadiusScale, lengthScale, kRadiusScale};
		worldTransform_.translation_ = {center.x, center.y - kModelCenterY * lengthScale, center.z};

	} else {
		// L0が斜めに置かれている
		assert(false && "L0 must be placed horizontally or vertically");
	}

	// 向きと長さを設定し終えた後のスケールを、再出現時の基準として保存する。
	baseScale_ = worldTransform_.scale_;

	UpdateWorldTransform(worldTransform_);
}

void Lazer::Update() {
	// レーザーの長さ方向（モデルのローカルY軸）を中心に常時回転させる。
	// 横レーザーはこの後のZ回転と合成されるため、縦・横どちらも向きを保ったまま回る。
	worldTransform_.rotation_.y += kRotationSpeed;
	if (worldTransform_.rotation_.y >= std::numbers::pi_v<float> * 2.0f) {
		worldTransform_.rotation_.y -= std::numbers::pi_v<float> * 2.0f;
	}

	UpdateBehavior();
	UpdateWorldTransform(worldTransform_);
}

void Lazer::Draw() {
	if (behavior_ == Behavior::kInactive) {
		return;
	}

	model_->Draw(worldTransform_, *camera_, &color_);
}

void Lazer::SetActive(bool isActive) {
	if (desiredActive_ == isActive) {
		return;
	}

	desiredActive_ = isActive;

	if (desiredActive_) {
		behaviorRequest_ = Behavior::kAppearing;
	} else {
		behaviorRequest_ = Behavior::kDisappearing;
	}
}

void Lazer::UpdateBehavior() {
	if (behaviorRequest_) {
		behavior_ = behaviorRequest_.value();

		switch (behavior_) {
		case Behavior::kActive:
			BehaviorActiveInitialize();
			break;
		case Behavior::kDisappearing:
			BehaviorDisappearingInitialize();
			break;
		case Behavior::kInactive:
			BehaviorInactiveInitialize();
			break;
		case Behavior::kAppearing:
			BehaviorAppearingInitialize();
			break;
		}

		behaviorRequest_ = std::nullopt;
	}

	switch (behavior_) {
	case Behavior::kActive:
		BehaviorActiveUpdate();
		break;
	case Behavior::kDisappearing:
		BehaviorDisappearingUpdate();
		break;
	case Behavior::kInactive:
		BehaviorInactiveUpdate();
		break;
	case Behavior::kAppearing:
		BehaviorAppearingUpdate();
		break;
	}
}

void Lazer::BehaviorActiveInitialize() {
	isActive_ = true;
	worldTransform_.scale_ = baseScale_;
}

void Lazer::BehaviorActiveUpdate() {
	effectTime_ += 1.0f / 60.0f;

	const float pulse = (std::sin(effectTime_ * 8.0f) + 1.0f) * 0.5f;

	color_.SetColor({
	    1.0f,
	    0.05f + pulse * 0.25f,
	    0.05f + pulse * 0.15f,
	    1.0f,
	});
}

void Lazer::BehaviorDisappearingInitialize() { behaviorTimer_ = 0.0f; }

void Lazer::BehaviorDisappearingUpdate() {
	behaviorTimer_ += 1.0f / 60.0f;

	float t = behaviorTimer_ / kAnimationDuration;
	t = (std::min)(t, 1.0f);

	worldTransform_.scale_.x = baseScale_.x * (1.0f - t);
	worldTransform_.scale_.z = baseScale_.z * (1.0f - t);

	if (t >= 1.0f) {
		isActive_ = false;
		behaviorRequest_ = Behavior::kInactive;
	}
}

void Lazer::BehaviorInactiveInitialize() {
	worldTransform_.scale_.x = 0.0f;
	worldTransform_.scale_.z = 0.0f;
}

void Lazer::BehaviorInactiveUpdate() {}

void Lazer::BehaviorAppearingInitialize() { behaviorTimer_ = 0.0f; }

void Lazer::BehaviorAppearingUpdate() {
	behaviorTimer_ += 1.0f / 60.0f;

	float t = behaviorTimer_ / kAnimationDuration;
	t = (std::min)(t, 1.0f);

	worldTransform_.scale_.x = baseScale_.x * t;
	worldTransform_.scale_.z = baseScale_.z * t;

	if (t >= 1.0f) {
		isActive_ = true;
		behaviorRequest_ = Behavior::kActive;
	}
}
