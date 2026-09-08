#include "ElectricPlatform.h"
#include "WorldTransformConfig.h"
#include "math/MathUtility.h"
#include <cassert>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

void ElectricPlatform::Initialize(
    Model* model, Camera* camera, const Vector3& start, const Vector3& end, uint8_t id) {
	assert(model);
	assert(camera);

	model_ = model;
	camera_ = camera;
	start_ = start;
	moveStep_ = end - start;
	forwardTarget_ = start;
	id_ = id;

	worldTransform_.Initialize();
	worldTransform_.translation_ = start_;
	worldTransform_.scale_ = {kWidth, kHeight, 1.0f};

	color_.Initialize();
	color_.SetColor(kIdleColor);
	UpdateWorldTransform(worldTransform_);
}

void ElectricPlatform::Update() {
	const Vector3 previousPosition = worldTransform_.translation_;

	if (isMovingForward_) {
		// 1回以上の帯電で積み上げられた目標地点まで進む。
		Vector3 difference = forwardTarget_ - worldTransform_.translation_;
		const float distance = Length(difference);

		if (distance <= kMoveSpeed) {
			worldTransform_.translation_ = forwardTarget_;
			isMovingForward_ = false;
			returnWaitTimer_ = kReturnWaitFrames;
		} else {
			Normalize(difference);
			worldTransform_.translation_ = worldTransform_.translation_ + difference * kMoveSpeed;
		}
	} else if (returnWaitTimer_ > 0) {
		// 最後の移動が完了してから一定時間その場に留まる。
		--returnWaitTimer_;
		if (returnWaitTimer_ == 0) {
			isReturning_ = true;
		}
	} else if (isReturning_) {
		// 待機時間中に再帯電しなかった場合は始点へ戻る。
		Vector3 difference = start_ - worldTransform_.translation_;
		const float distance = Length(difference);

		if (distance <= kMoveSpeed) {
			worldTransform_.translation_ = start_;
			forwardTarget_ = start_;
			isReturning_ = false;
		} else {
			Normalize(difference);
			worldTransform_.translation_ = worldTransform_.translation_ + difference * kMoveSpeed;
		}
	}

	moveDelta_ = worldTransform_.translation_ - previousPosition;
	color_.SetColor(IsCharged() ? kChargedColor : kIdleColor);
	UpdateWorldTransform(worldTransform_);
}

void ElectricPlatform::Draw() { model_->Draw(worldTransform_, *camera_, &color_); }

void ElectricPlatform::Charge() {
	// 帯電1回につき、CSVで指定された方向・距離ぶん目標位置を先へ延ばす。
	// 帰還中に帯電した場合は、現在位置を基準に新しい1回分を進む。
	if (isReturning_) {
		forwardTarget_ = worldTransform_.translation_;
	}
	forwardTarget_ = forwardTarget_ + moveStep_;
	isMovingForward_ = true;
	isReturning_ = false;
	returnWaitTimer_ = 0;
}

MapChipField::Rect ElectricPlatform::GetRect() const {
	const Vector3& position = worldTransform_.translation_;
	return {
	    position.x - kWidth / 2.0f, position.x + kWidth / 2.0f,
	    position.y - kHeight / 2.0f, position.y + kHeight / 2.0f};
}
