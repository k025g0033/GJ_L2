#pragma once

#include "KamataEngine.h"
#include "Player.h"
#include "MapChipField.h"
#include <cstdint>
#include <vector>
#include <optional>

// プレイヤーまたは変身済みクローンが上に乗ると作動する感圧板
class PushPlate {
public:
	void Initialize(
	    KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position,
	    uint8_t id, uint8_t requiredActorCount = 1, float width = 0.8f);
	void Update(const std::vector<Player*>& actors, const std::vector<MapChipField::Rect>& cloneBaseRects);
	void Draw();

	bool IsPushed() const { return isPushed_; }
	uint8_t GetID() const { return id_; }
	uint8_t GetRequiredActorCount() const { return requiredActorCount_; }
	uint8_t GetCurrentActorCount() const { return currentActorCount_; }

	bool IsStandingOn(const MapChipField::Rect& actorRect) const;

private:

	/// 演出 ///
	enum class Behavior {
		kIdle,
		kPressing,
		kPressed,
		kReleasing,
	};

	Behavior behavior_ = Behavior::kIdle;
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	float behaviorTimer_ = 0.0f;

	static inline const float kAnimationDuration = 0.15f;

	void UpdateBehavior();

	void BehaviorIdleInitialize();
	void BehaviorIdleUpdate();

	void BehaviorPressingInitialize();
	void BehaviorPressingUpdate();

	void BehaviorPressedInitialize();
	void BehaviorPressedUpdate();

	void BehaviorReleasingInitialize();
	void BehaviorReleasingUpdate();

	/// ///

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

	static inline const float kHeight = 0.18f;
	static inline const float kPushedHeight = 0.04f;
	static inline const float kStandingTolerance = 0.15f;
	static inline const KamataEngine::Vector4 kIdleColor = {1.0f, 0.45f, 0.05f, 1.0f};
	static inline const KamataEngine::Vector4 kPushedColor = {0.2f, 1.0f, 0.25f, 1.0f};
};
