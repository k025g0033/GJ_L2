#include "ChargePoint.h"
#include "WorldTransformConfig.h"
#include <cassert>
#include <cmath>

using namespace KamataEngine;

void ChargePoint::Initialize(Model* model, Camera* camera, const Vector3& position) {

	assert(model);
	assert(camera);

	model_ = model;
	camera_ = camera;

	worldTransform_.Initialize();
	// このOBJの原点はモデルの底面にあるため、マップ1マスの下端へ合わせる。
	worldTransform_.translation_ = {
	    position.x,
	    position.y - MapChipField::kBlockHeight / 2.0f,
	    position.z};

	// 縦横奥行きを同じ倍率にし、元モデルの形を崩さず1ブロック内へ収める。
	worldTransform_.scale_ = {kModelScale, kModelScale, kModelScale};

	color_.Initialize();
	color_.SetColor({0.1f, 0.8f, 1.0f, 1.0f});

	UpdateWorldTransform(worldTransform_);
}

void ChargePoint::Update() {
	effectTime_ += 1.0f / 60.0f;
	// 常にゆっくり点滅する（約3秒で1往復）。
	const float pulse = (std::sin(effectTime_ * 2.0f) + 1.0f) * 0.5f;

	// 帯電中のクローンと同系統の青白い明滅。
	color_.SetColor({
	    0.15f + pulse * 0.75f,
	    0.35f + pulse * 0.65f,
	    0.65f + pulse * 0.35f,
	    1.0f});

	// エネルギーが脈打って見える程度に、ごく小さく拡縮する。
	const float scalePulse = kModelScale * (0.97f + pulse * 0.03f);
	worldTransform_.scale_ = {scalePulse, scalePulse, scalePulse};

	UpdateWorldTransform(worldTransform_);
}

void ChargePoint::Draw() { model_->Draw(worldTransform_, *camera_, &color_); }

MapChipField::Rect ChargePoint::GetRect() const {
	const Vector3& position = worldTransform_.translation_;

	return {
	    position.x - kWidth / 2.0f,
	    position.x + kWidth / 2.0f,
	    position.y,
	    position.y + kHeight,
	};
}
