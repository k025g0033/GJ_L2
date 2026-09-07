#pragma once
#include "KamataEngine.h"
#include "Player.h"
#include<cstdint>

    class Key {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position, uint8_t id);

	void Update(const Player* player);
	void Draw();

	bool IsCollected() const { return isCollected_; }
	uint8_t GetID() const { return id_; }

private:
	bool IsCollidingWithPlayer(const Player* player) const;

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	uint8_t id_ = 0;
	bool isCollected_ = false;

	static inline const float kSize = 0.5f;
};