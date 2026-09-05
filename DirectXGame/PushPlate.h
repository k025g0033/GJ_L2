#pragma once

#include "KamataEngine.h"
#include "Player.h"
#include <cstdint>
#include <vector>

// プレイヤーまたは変身済みクローンが上に乗ると作動する感圧板
class PushPlate {
public:
	void Initialize(
	    KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position,
	    uint8_t id, uint8_t requiredActorCount = 1, float width = 0.8f);
	void Update(const std::vector<Player*>& actors);
	void Draw();

	bool IsPushed() const { return isPushed_; }
	uint8_t GetID() const { return id_; }
	uint8_t GetRequiredActorCount() const { return requiredActorCount_; }
	uint8_t GetCurrentActorCount() const { return currentActorCount_; }

private:
	bool IsStandingOn(const Player* actor) const;

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	uint8_t id_ = 0;
	uint8_t requiredActorCount_ = 1;
	uint8_t currentActorCount_ = 0;
	float width_ = 0.8f;
	bool isPushed_ = false;

	static inline const float kHeight = 0.1f;
	static inline const float kPushedHeight = 0.04f;
	static inline const float kStandingTolerance = 0.15f;
	static inline const KamataEngine::Vector4 kIdleColor = {1.0f, 0.45f, 0.05f, 1.0f};
	static inline const KamataEngine::Vector4 kPushedColor = {0.2f, 1.0f, 0.25f, 1.0f};
};
