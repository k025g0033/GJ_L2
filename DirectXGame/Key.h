#pragma once
#include "KamataEngine.h"
#include "MapChipField.h"
#include "Player.h"
#include<cstdint>

    class Key {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position, uint8_t id);

	void Update(const Player* player);
	void Draw();

	bool IsCollected() const { return isCollected_; }
	uint8_t GetID() const { return id_; }
	MapChipField::Rect GetRect() const;

private:
	bool IsCollidingWithPlayer(const Player* player) const;

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	uint8_t id_ = 0;
	bool isCollected_ = false;
	bool isCollecting_ = false;
	float collectAnimationTimer_ = 0.0f;
	KamataEngine::Vector3 collectStartPosition_{};

	static inline const float kSize = 0.5f;
	static inline const float kPickupMargin = 0.1f;
	static inline const float kCollectAnimationDuration = 0.45f;
	static inline const float kCollectRiseDistance = 0.7f;
};
