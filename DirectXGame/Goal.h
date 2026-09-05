#pragma once
#include "KamataEngine.h"
#include "Player.h"

class Goal {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position);

	void Update(const Player* player);
	void Draw();

	bool IsReached() const { return isReached_; }

private:
	bool IsCollidingWithPlayer(const Player* player) const;

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	bool isReached_ = false;

	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 1.5f;

	static inline const KamataEngine::Vector4 kGoalColor = {1.0f, 0.8f, 0.1f, 1.0f};
};