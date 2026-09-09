#define NOMINMAX
#include "PushPlate.h"
#include "WorldTransformConfig.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

void PushPlate::Initialize(
    Model* baseModel, Model* buttonModel, Camera* camera, const Vector3& position, uint8_t id, uint8_t requiredActorCount, float width,
    Model* countXModel, const std::array<Model*, 10>& countNumberModels) {
	baseModel_ = baseModel;
	buttonModel_ = buttonModel;
	countXModel_ = countXModel;
	countNumberModels_ = countNumberModels;
	camera_ = camera;
	id_ = id;
	requiredActorCount_ = std::max<uint8_t>(requiredActorCount, 1);
	width_ = std::max(width, 0.1f);

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.translation_.y -= 0.45f;
	worldTransform_.scale_ = {width_, kHeight, 0.8f};
	UpdateWorldTransform(worldTransform_);

	// Blenderで作ったモデルのサイズをそのまま使用する。
	baseWorldTransform_.Initialize();
	baseWorldTransform_.translation_ = position;
	baseWorldTransform_.translation_.y -= 0.5f;
	baseWorldTransform_.scale_ = {width_ / 2.0f, 1.0f, 0.5f};
	UpdateWorldTransform(baseWorldTransform_);

	buttonWorldTransform_.Initialize();
	buttonWorldTransform_.translation_ = baseWorldTransform_.translation_;
	buttonWorldTransform_.scale_ = baseWorldTransform_.scale_;
	UpdateWorldTransform(buttonWorldTransform_);

	// 感圧板全体の中央上へ「× 残り人数」を並べる。
	countXWorldTransform_.Initialize();
	countXWorldTransform_.translation_ = {position.x - kCountModelSpacing, position.y + kCountModelHeight, position.z};
	countXWorldTransform_.scale_ = {kCountModelScale, kCountModelScale, kCountModelScale};
	UpdateWorldTransform(countXWorldTransform_);

	countNumberWorldTransform_.Initialize();
	countNumberWorldTransform_.translation_ = {position.x + kCountModelSpacing, position.y + kCountModelHeight, position.z};
	countNumberWorldTransform_.rotation_.y = std::numbers::pi_v<float>;
	countNumberWorldTransform_.scale_ = {kCountModelScale, kCountModelScale, kCountModelScale};
	UpdateWorldTransform(countNumberWorldTransform_);

	baseColor_.Initialize();
	baseColor_.SetColor(kBaseColor);
	buttonColor_.Initialize();
	buttonColor_.SetColor(kButtonColor);
}

void PushPlate::Update(const std::vector<Player*>& actors, const std::vector<MapChipField::Rect>& cloneBaseRects) {
	currentActorCount_ = 0;

	 // 本体プレイヤーと変身済みクローン
	for (const Player* actor : actors) {
		if (actor != nullptr && IsStandingOn(actor)) {
			++currentActorCount_;
		}
	}

	// 変身前のクローンの素
	for (const MapChipField::Rect& rect : cloneBaseRects) {
		if (IsStandingOn(rect)) {
			++currentActorCount_;
		}
	}
	const bool shouldBePushed = currentActorCount_ >= requiredActorCount_;

	if (shouldBePushed != isPushed_) {
		isPushed_ = shouldBePushed;

		behaviorRequest_ = isPushed_ ? Behavior::kPressing : Behavior::kReleasing;
	}

	UpdateBehavior();
	UpdateWorldTransform(worldTransform_);
	UpdateWorldTransform(baseWorldTransform_);
	UpdateWorldTransform(buttonWorldTransform_);
	UpdateWorldTransform(countXWorldTransform_);
	UpdateWorldTransform(countNumberWorldTransform_);
}

bool PushPlate::IsStandingOn(const Player* actor) const {
	const Vector3& actorPosition = actor->GetWorldTransform().translation_;

	const float actorLeft = actorPosition.x - actor->GetWidth() / 2.0f;
	const float actorRight = actorPosition.x + actor->GetWidth() / 2.0f;
	const float actorBottom = actorPosition.y - actor->GetHeight() / 2.0f;
	const MapChipField::Rect plateRect = GetRect();
	const float plateLeft = plateRect.left;
	const float plateRight = plateRect.right;
	const float plateTop = plateRect.top;

	const bool overlapsX = actorRight > plateLeft && actorLeft < plateRight;
	const bool touchesTop = std::abs(actorBottom - plateTop) <= kStandingTolerance;
	return overlapsX && touchesTop;
}

void PushPlate::Draw() {
	baseModel_->Draw(baseWorldTransform_, *camera_, &baseColor_);
	buttonModel_->Draw(buttonWorldTransform_, *camera_, &buttonColor_);
	if (countXModel_ != nullptr) {
		countXModel_->Draw(countXWorldTransform_, *camera_);
	}
	const uint8_t remainingCount = currentActorCount_ >= requiredActorCount_ ? 0 : requiredActorCount_ - currentActorCount_;
	if (countNumberModels_[remainingCount] != nullptr) {
		countNumberModels_[remainingCount]->Draw(countNumberWorldTransform_, *camera_);
	}
}

bool PushPlate::IsStandingOn(const MapChipField::Rect& actorRect) const {
	const MapChipField::Rect plateRect = GetRect();
	float plateLeft = plateRect.left;
	float plateRight = plateRect.right;
	float plateTop = plateRect.top;

	bool overlapsX = actorRect.right > plateLeft && actorRect.left < plateRight;

	bool touchesTop = std::abs(actorRect.bottom - plateTop) <= kStandingTolerance;

	return overlapsX && touchesTop;
}

MapChipField::Rect PushPlate::GetRect() const {
	const float baseTop = baseWorldTransform_.translation_.y + kBaseModelTop;
	const float buttonTop = buttonWorldTransform_.translation_.y + kButtonModelTop;
	return {
	    baseWorldTransform_.translation_.x - width_ / 2.0f,
	    baseWorldTransform_.translation_.x + width_ / 2.0f,
	    baseWorldTransform_.translation_.y,
	    (std::max)(baseTop, buttonTop),
	};
}

void PushPlate::UpdateBehavior() {
	if (behaviorRequest_) {
		behavior_ = behaviorRequest_.value();

		switch (behavior_) {
		case Behavior::kIdle:
			BehaviorIdleInitialize();
			break;
		case Behavior::kPressing:
			BehaviorPressingInitialize();
			break;
		case Behavior::kPressed:
			BehaviorPressedInitialize();
			break;
		case Behavior::kReleasing:
			BehaviorReleasingInitialize();
			break;
		}

		behaviorRequest_ = std::nullopt;
	}

	switch (behavior_) {
	case Behavior::kIdle:
		BehaviorIdleUpdate();
		break;
	case Behavior::kPressing:
		BehaviorPressingUpdate();
		break;
	case Behavior::kPressed:
		BehaviorPressedUpdate();
		break;
	case Behavior::kReleasing:
		BehaviorReleasingUpdate();
		break;
	}
}

void PushPlate::BehaviorIdleInitialize() { buttonWorldTransform_.translation_.y = baseWorldTransform_.translation_.y; }

void PushPlate::BehaviorIdleUpdate() {}

void PushPlate::BehaviorPressingInitialize() { behaviorTimer_ = 0.0f; }

void PushPlate::BehaviorPressingUpdate() {
	behaviorTimer_ += 1.0f / 60.0f;

	float t = behaviorTimer_ / kAnimationDuration;
	t = (std::min)(t, 1.0f);

	buttonWorldTransform_.translation_.y = baseWorldTransform_.translation_.y + kButtonPressedOffset * t;

	if (t >= 1.0f) {
		behaviorRequest_ = Behavior::kPressed;
	}
}

void PushPlate::BehaviorPressedInitialize() { buttonWorldTransform_.translation_.y = baseWorldTransform_.translation_.y + kButtonPressedOffset; }

void PushPlate::BehaviorPressedUpdate() {}

void PushPlate::BehaviorReleasingInitialize() { behaviorTimer_ = 0.0f; }

void PushPlate::BehaviorReleasingUpdate() {
	behaviorTimer_ += 1.0f / 60.0f;

	float t = behaviorTimer_ / kAnimationDuration;
	t = (std::min)(t, 1.0f);

	buttonWorldTransform_.translation_.y = baseWorldTransform_.translation_.y + kButtonPressedOffset * (1.0f - t);

	if (t >= 1.0f) {
		behaviorRequest_ = Behavior::kIdle;
	}
}
