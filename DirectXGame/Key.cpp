#include "Key.h"
#include "WorldTransformConfig.h"
#include <algorithm>

using namespace KamataEngine;

void Key::Initialize(Model* model, Camera* camera, const Vector3& position, uint8_t id) {

	model_ = model;
	camera_ = camera;
	id_ = id;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	// 鍵モデルの原点は底面にあるため、CSVマス中心から半マス下げて床上面へ合わせる。
	worldTransform_.translation_.y -= MapChipField::kBlockHeight / 2.0f;
	worldTransform_.scale_ = {kSize, kSize, kSize};

	color_.Initialize();
	// 色はKey.mtlから各パーツのテクスチャとして読み込む。
	color_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});

	UpdateWorldTransform(worldTransform_);
}

void Key::Update(const Player* player) {
	if (isCollecting_) {
		collectAnimationTimer_ += 1.0f / 60.0f;
		float t = std::clamp(collectAnimationTimer_ / kCollectAnimationDuration, 0.0f, 1.0f);
		const float easedT = 1.0f - (1.0f - t) * (1.0f - t);

		// 上へ浮きながら高速回転し、最後に小さくなって消える。
		worldTransform_.translation_.y = collectStartPosition_.y + kCollectRiseDistance * easedT;
		worldTransform_.rotation_.y += 0.22f;
		const float scale = kSize * (1.0f - easedT);
		worldTransform_.scale_ = {scale, scale, scale};
		// RGBは変えず、取得アニメーションでは透明度だけを下げる。
		color_.SetColor({1.0f, 1.0f, 1.0f, 1.0f - easedT});

		if (t >= 1.0f) {
			isCollecting_ = false;
		}
		UpdateWorldTransform(worldTransform_);
		return;
	}

	if (isCollected_) {
		return;
	}

	if (IsCollidingWithPlayer(player)) {
		isCollected_ = true;
		isCollecting_ = true;
		collectAnimationTimer_ = 0.0f;
		collectStartPosition_ = worldTransform_.translation_;
	}

	// 鍵だと分かりやすいように回転
	worldTransform_.rotation_.y += 0.04f;
	UpdateWorldTransform(worldTransform_);
}

void Key::Draw() {
	if (!isCollected_ || isCollecting_) {
		model_->Draw(worldTransform_, *camera_, &color_);
	}
}

MapChipField::Rect Key::GetRect() const {
	const Vector3& position = worldTransform_.translation_;
	const float halfSize = kSize / 2.0f;
	return {
	    position.x - halfSize,
	    position.x + halfSize,
	    position.y - halfSize,
	    position.y + halfSize,
	};
}

bool Key::IsCollidingWithPlayer(const Player* player) const {
	const Vector3& playerPosition = player->GetWorldTransform().translation_;

	const Vector3& keyPosition = worldTransform_.translation_;

	// 物理判定で表面に止まっていても拾えるよう、取得範囲だけ少し広げる。
	float halfKey = kSize / 2.0f + kPickupMargin;

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
