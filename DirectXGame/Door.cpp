#include "Door.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;

void Door::Initialize(Model* model, Camera* camera, const Vector3& position, uint8_t id) {
	model_ = model;
	camera_ = camera;
	id_ = id;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	UpdateWorldTransform(worldTransform_);
	color_.Initialize();
	color_.SetColor(kClosedColor);
}

void Door::Update() { UpdateWorldTransform(worldTransform_); }

void Door::Draw() {
	if (!isOpen_) {
		model_->Draw(worldTransform_, *camera_, &color_);
	}
}

MapChipField::Rect Door::GetRect() const {
	const Vector3& position = worldTransform_.translation_;
	return {
	    position.x - MapChipField::kBlockWidth / 2.0f,
	    position.x + MapChipField::kBlockWidth / 2.0f,
	    position.y - MapChipField::kBlockHeight / 2.0f,
	    position.y + MapChipField::kBlockHeight / 2.0f,
	};
}
