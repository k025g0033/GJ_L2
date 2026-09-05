#define NOMINMAX
#include "PushPlate.h"
#include "WorldTransformConfig.h"
#include <algorithm>
#include <cmath>

using namespace KamataEngine;

void PushPlate::Initialize(
    Model* model, Camera* camera, const Vector3& position, uint8_t id, uint8_t requiredActorCount, float width) {
	model_ = model;
	camera_ = camera;
	id_ = id;
	requiredActorCount_ = std::max<uint8_t>(requiredActorCount, 1);
	width_ = std::max(width, 0.1f);

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.translation_.y -= 0.45f;
	worldTransform_.scale_ = {width_, kHeight, 0.8f};
	UpdateWorldTransform(worldTransform_);
	color_.Initialize();
	color_.SetColor(kIdleColor);
}

void PushPlate::Update(const std::vector<Player*>& actors) {
	currentActorCount_ = 0;
	for (const Player* actor : actors) {
		if (actor != nullptr && IsStandingOn(actor)) {
			++currentActorCount_;
		}
	}
	isPushed_ = currentActorCount_ >= requiredActorCount_;

	worldTransform_.scale_.y = isPushed_ ? kPushedHeight : kHeight;
	color_.SetColor(isPushed_ ? kPushedColor : kIdleColor);
	UpdateWorldTransform(worldTransform_);
}

bool PushPlate::IsStandingOn(const Player* actor) const {
	const Vector3& actorPosition = actor->GetWorldTransform().translation_;
	const Vector3& platePosition = worldTransform_.translation_;

	const float actorLeft = actorPosition.x - actor->GetWidth() / 2.0f;
	const float actorRight = actorPosition.x + actor->GetWidth() / 2.0f;
	const float actorBottom = actorPosition.y - actor->GetHeight() / 2.0f;
	const float plateLeft = platePosition.x - width_ / 2.0f;
	const float plateRight = platePosition.x + width_ / 2.0f;
	const float plateTop = platePosition.y + kHeight / 2.0f;

	const bool overlapsX = actorRight > plateLeft && actorLeft < plateRight;
	const bool touchesTop = std::abs(actorBottom - plateTop) <= kStandingTolerance;
	return overlapsX && touchesTop;
}

void PushPlate::Draw() { model_->Draw(worldTransform_, *camera_, &color_); }
