#include "Goal.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;

void Goal::Initialize(Model* model, Camera* camera, const Vector3& position) {

	model_ = model;
	camera_ = camera;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;

	// 縦長にして通常ブロックと区別する
	worldTransform_.scale_ = {kWidth, kHeight, 0.8f};

	color_.Initialize();
	color_.SetColor(kGoalColor);

	UpdateWorldTransform(worldTransform_);
}

void Goal::Update(const Player* player) {
	isReached_ = IsCollidingWithPlayer(player);

	// 仮の回転演出
	worldTransform_.rotation_.y += 0.02f;

	UpdateWorldTransform(worldTransform_);
}

bool Goal::IsCollidingWithPlayer(const Player* player) const {

	const Vector3& playerPosition = player->GetWorldTransform().translation_;

	const Vector3& goalPosition = worldTransform_.translation_;

	float playerLeft = playerPosition.x - player->GetWidth() / 2.0f;

	float playerRight = playerPosition.x + player->GetWidth() / 2.0f;

	float playerBottom = playerPosition.y - player->GetHeight() / 2.0f;

	float playerTop = playerPosition.y + player->GetHeight() / 2.0f;

	float goalLeft = goalPosition.x - kWidth / 2.0f;

	float goalRight = goalPosition.x + kWidth / 2.0f;

	float goalBottom = goalPosition.y - kHeight / 2.0f;

	float goalTop = goalPosition.y + kHeight / 2.0f;

	return playerRight > goalLeft && playerLeft < goalRight && playerTop > goalBottom && playerBottom < goalTop;
}

void Goal::Draw() { model_->Draw(worldTransform_, *camera_, &color_); }