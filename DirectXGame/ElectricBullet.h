#pragma once

#include "KamataEngine.h"
#include "MapChipField.h"

class ElectricBullet {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position, const KamataEngine::Vector3& velocity);

	void Update(MapChipField* mapChipField);
	void Draw();

	bool IsDead() const { return isDead_; }

	MapChipField::Rect GetRect() const {
		const KamataEngine::Vector3& position = worldTransform_.translation_;

		return {position.x - kCollisionRadius, position.x + kCollisionRadius, position.y - kCollisionRadius, position.y + kCollisionRadius};
	}

	void SetDead() { isDead_ = true; }

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	// 毎フレームの移動量
	KamataEngine::Vector3 velocity_ = {};

	// 削除してよいか
	bool isDead_ = false;

	// 約3秒後に消える
	int lifeTimer_ = 60 * 3;

	static inline const float kScale = 0.2f;

	static inline const float kCollisionRadius = 0.15f;
};
