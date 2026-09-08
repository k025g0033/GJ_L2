#include "Door.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;

void Door::Initialize(Model* model, Camera* camera, const Vector3& position, uint8_t id) {
	model_ = model;
	camera_ = camera;
	id_ = id;

	closedPosition_ = position;
	openPosition_ = position;
	openPosition_.y += kOpenDistance;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
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
	const Vector3& position = worldTransform_.translation_;
	return {
	    position.x - MapChipField::kBlockWidth / 2.0f,
	    position.x + MapChipField::kBlockWidth / 2.0f,
	    position.y - MapChipField::kBlockHeight / 2.0f,
	    position.y + MapChipField::kBlockHeight / 2.0f,
	};
}

bool Door::IsCollidingWithPlayer(const Player* player) const {
	MapChipField::Rect rect = {
	    closedPosition_.x - MapChipField::kBlockWidth / 2.0f,
	    closedPosition_.x + MapChipField::kBlockWidth / 2.0f,
	    closedPosition_.y - MapChipField::kBlockHeight / 2.0f,
	    closedPosition_.y + MapChipField::kBlockHeight / 2.0f,
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

	worldTransform_.translation_.y =
	    closedPosition_.y +
	    (openPosition_.y - closedPosition_.y) * t;

	if (behaviorTimer_ >= kAnimationDuration) {
		isOpen_ = true;
		behaviorRequest_ = Behavior::kOpen;
	}
}

void Door::BehaviorClosedInitialize() {
	isOpen_ = false;
	worldTransform_.translation_ = closedPosition_;
}

void Door::BehaviorClosedUpdate() {}

void Door::BehaviorOpenInitialize() {
	isOpen_ = true;
	worldTransform_.translation_ = openPosition_;
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

	worldTransform_.translation_.y = openPosition_.y + (closedPosition_.y - openPosition_.y) * t;

	if (behaviorTimer_ >= kAnimationDuration) {
		behaviorRequest_ = Behavior::kClosed;
	}
}