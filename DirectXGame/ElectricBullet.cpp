#include "ElectricBullet.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;

void ElectricBullet::Initialize(Model* model, Camera* camera, const Vector3& position, const Vector3& velocity) {

	model_ = model;
	camera_ = camera;
	velocity_ = velocity;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kScale, kScale, kScale};

	color_.Initialize();
	color_.SetColor({0.2f, 0.8f, 1.0f, 1.0f});

	UpdateWorldTransform(worldTransform_);
}

void ElectricBullet::Update(MapChipField* mapChipField) {
	if (isDead_) {
		return;
	}

	// 速度分だけ移動
	worldTransform_.translation_.x += velocity_.x;
	worldTransform_.translation_.y += velocity_.y;
	worldTransform_.translation_.z += velocity_.z;

	// 現在位置のCSVマスを取得
	MapChipField::IndexSet index = mapChipField->GetMapChipIndexByPosition(worldTransform_.translation_);

	// ブロックへ当たったら消える
	if (mapChipField->GetMapChipTypeByIndex(index.xIndex, index.yIndex) == MapChipType::kBlock) {

		isDead_ = true;
		return;
	}

	--lifeTimer_;

	if (lifeTimer_ <= 0) {
		isDead_ = true;
	}

	UpdateWorldTransform(worldTransform_);
}

void ElectricBullet::Draw() {
	if (!isDead_) {
		model_->Draw(worldTransform_, *camera_, &color_);
	}
}