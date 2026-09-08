#include "Door.h"
#include "WorldTransformConfig.h"
#include <cmath>
#include <numbers>

using namespace KamataEngine;

void Door::Initialize(Model* model, Camera* camera, const Vector3& position, uint8_t id) {
	model_ = model;
	camera_ = camera;
	id_ = id;

	// OBJの原点は底面にあるので、CSVマス中心から半マス下げて床上面へ合わせる。
	closedPosition_ = position;
	closedPosition_.y -= MapChipField::kBlockHeight / 2.0f;
	// 画面から見て扉の右下を蝶番の位置にする。
	hingePosition_ = closedPosition_;
	hingePosition_.x += kDoorHalfWidth;
	closedRotationY_ = -std::numbers::pi_v<float> / 2.0f;
	// 閉じた向きから約60度だけ開く。
	openRotationY_ = closedRotationY_ + std::numbers::pi_v<float> / 3.0f;

	worldTransform_.Initialize();
	// Blenderモデルは約2マス角で横幅がZ方向なので、1マスへ縮小して正面をカメラへ向ける。
	worldTransform_.scale_ = {0.5f, 0.5f, 0.5f};
	ApplyHingeRotation(closedRotationY_);
	UpdateWorldTransform(worldTransform_);
	color_.Initialize();
	color_.SetColor(kClosedColor);
}

void Door::Update() {
	UpdateBehavior();
	UpdateWorldTransform(worldTransform_);
}

void Door::Draw() { model_->Draw(worldTransform_, *camera_, &color_); }

MapChipField::Rect Door::GetRect() const {
	// 開閉演出中も、閉じている扉の当たり判定は入口から動かさない。
	const Vector3& position = closedPosition_;
	const float centerY = position.y + kCollisionCenterOffsetY;
	return {
	    position.x - MapChipField::kBlockWidth / 2.0f,
	    position.x + MapChipField::kBlockWidth / 2.0f,
	    centerY - MapChipField::kBlockHeight / 2.0f,
	    centerY + MapChipField::kBlockHeight / 2.0f,
	};
}

void Door::ApplyHingeRotation(float rotationY) {
	// 閉じた状態では、モデル原点は右下の蝶番から半マス左にある。
	// その距離を保ったままY軸回転させることで、右下を固定して開閉する。
	const float deltaRotation = rotationY - closedRotationY_;
	const float cosine = std::cos(deltaRotation);
	const float sine = std::sin(deltaRotation);

	worldTransform_.translation_.x = hingePosition_.x - kDoorHalfWidth * cosine;
	worldTransform_.translation_.y = hingePosition_.y;
	worldTransform_.translation_.z = hingePosition_.z + kDoorHalfWidth * sine;
	worldTransform_.rotation_.y = rotationY;
}

bool Door::IsCollidingWithPlayer(const Player* player) const {
	MapChipField::Rect rect = {
	    closedPosition_.x - MapChipField::kBlockWidth / 2.0f,
	    closedPosition_.x + MapChipField::kBlockWidth / 2.0f,
	    closedPosition_.y + kCollisionCenterOffsetY - MapChipField::kBlockHeight / 2.0f,
	    closedPosition_.y + kCollisionCenterOffsetY + MapChipField::kBlockHeight / 2.0f,
	};

	const Vector3& position = player->GetWorldTransform().translation_;

	return position.x + player->GetWidth() / 2.0f > rect.left && position.x - player->GetWidth() / 2.0f < rect.right && position.y + player->GetHeight() / 2.0f > rect.bottom &&
	       position.y - player->GetHeight() / 2.0f < rect.top;
}

void Door::SetOpen(bool isOpen) {
	if (desiredOpen_ == isOpen) {
		return;
	}

	desiredOpen_ = isOpen;

	behaviorRequest_ = desiredOpen_ ? Behavior::kOpening : Behavior::kClosing;
}

void Door::UpdateBehavior() {
	if (behaviorRequest_) {
		behavior_ = behaviorRequest_.value();

		switch (behavior_) {
		case Behavior::kClosed:
			BehaviorClosedInitialize();
			break;

		case Behavior::kOpening:
			BehaviorOpeningInitialize();
			break;

		case Behavior::kOpen:
			BehaviorOpenInitialize();
			break;

		case Behavior::kClosing:
			BehaviorClosingInitialize();
			break;
		}

		behaviorRequest_ = std::nullopt;
	}

	switch (behavior_) {
	case Behavior::kClosed:
		BehaviorClosedUpdate();
		break;

	case Behavior::kOpening:
		BehaviorOpeningUpdate();
		break;

	case Behavior::kOpen:
		BehaviorOpenUpdate();
		break;

	case Behavior::kClosing:
		BehaviorClosingUpdate();
		break;
	}
}

void Door::BehaviorOpeningInitialize() {
	behaviorTimer_ = 0.0f;
}

void Door::BehaviorOpeningUpdate() {
	behaviorTimer_ += 1.0f / 60.0f;

	float t = behaviorTimer_ / kAnimationDuration;
	t = (std::min)(t, 1.0f);

	// ゆっくり始まり、ゆっくり止まる
	t = t * t * (3.0f - 2.0f * t);

	ApplyHingeRotation(closedRotationY_ + (openRotationY_ - closedRotationY_) * t);

	if (behaviorTimer_ >= kAnimationDuration) {
		isOpen_ = true;
		behaviorRequest_ = Behavior::kOpen;
	}
}

void Door::BehaviorClosedInitialize() {
	isOpen_ = false;
	ApplyHingeRotation(closedRotationY_);
}

void Door::BehaviorClosedUpdate() {}

void Door::BehaviorOpenInitialize() {
	isOpen_ = true;
	ApplyHingeRotation(openRotationY_);
}

void Door::BehaviorOpenUpdate() {}

void Door::BehaviorClosingInitialize() {
	behaviorTimer_ = 0.0f;

	// 閉じ始めた時点から障害物へ戻す
	isOpen_ = false;
}

void Door::BehaviorClosingUpdate() {
	behaviorTimer_ += 1.0f / 60.0f;

	float t = behaviorTimer_ / kAnimationDuration;
	t = (std::min)(t, 1.0f);

	t = t * t * (3.0f - 2.0f * t);

	ApplyHingeRotation(openRotationY_ + (closedRotationY_ - openRotationY_) * t);

	if (behaviorTimer_ >= kAnimationDuration) {
		behaviorRequest_ = Behavior::kClosed;
	}
}
