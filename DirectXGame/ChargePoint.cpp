#include "ChargePoint.h"
#include "WorldTransformConfig.h"
#include <cassert>

using namespace KamataEngine;

void ChargePoint::Initialize(Model* model, Camera* camera, const Vector3& position) {

	assert(model);
	assert(camera);

	model_ = model;
	camera_ = camera;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;

	// 通常ブロックと区別するため少し小さくする
	worldTransform_.scale_ = {0.7f, 0.7f, 0.7f};

	color_.Initialize();
	color_.SetColor({0.1f, 0.8f, 1.0f, 1.0f});

	UpdateWorldTransform(worldTransform_);
}

void ChargePoint::Update() { UpdateWorldTransform(worldTransform_); }

void ChargePoint::Draw() { model_->Draw(worldTransform_, *camera_, &color_); }

MapChipField::Rect ChargePoint::GetRect() const {
	const Vector3& position = worldTransform_.translation_;

	return {
	    position.x - kWidth / 2.0f,
	    position.x + kWidth / 2.0f,
	    position.y - kHeight / 2.0f,
	    position.y + kHeight / 2.0f,
	};
}