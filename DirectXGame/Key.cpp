#include "Key.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;

void Key::Initialize(Model* model, Camera* camera, const Vector3& position, uint8_t id) {

	model_ = model;
	camera_ = camera;
	id_ = id;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kSize, kSize, kSize};

	color_.Initialize();
	color_.SetColor({1.0f, 0.85f, 0.05f, 1.0f});

	UpdateWorldTransform(worldTransform_);
}

void Key::Update(const Player* player) {
	if (isCollected_) {
		return;
	}

	if (IsCollidingWithPlayer(player)) {
		isCollected_ = true;
	}

	// 鍵だと分かりやすいように回転
	worldTransform_.rotation_.y += 0.04f;
	UpdateWorldTransform(worldTransform_);
}

void Key::Draw() {
	if (!isCollected_) {
		model_->Draw(worldTransform_, *camera_, &color_);
	}
}

bool Key::IsCollidingWithPlayer(const Player* player) const {
	const Vector3& playerPosition = player->GetWorldTransform().translation_;

	const Vector3& keyPosition = worldTransform_.translation_;

	float halfKey = kSize / 2.0f;

	float playerLeft = playerPosition.x - player->GetWidth() / 2.0f;
	float playerRight = playerPosition.x + player->GetWidth() / 2.0f;
	float playerBottom = playerPosition.y - player->GetHeight() / 2.0f;
	float playerTop = playerPosition.y + player->GetHeight() / 2.0f;

	float keyLeft = keyPosition.x - halfKey;
	float keyRight = keyPosition.x + halfKey;
	float keyBottom = keyPosition.y - halfKey;
	float keyTop = keyPosition.y + halfKey;

	return playerRight > keyLeft && playerLeft < keyRight && playerTop > keyBottom && playerBottom < keyTop;
}