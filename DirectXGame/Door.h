#pragma once

#include "KamataEngine.h"
#include "MapChipField.h"
#include "Player.h"
#include <cstdint>
#include <optional>

// 同じIDの感圧板から開閉状態を受け取る扉
class Door {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position, uint8_t id);
	void Update();
	void Draw();

	void SetOpen(bool isOpen);
	bool IsOpen() const { return isOpen_; }
	uint8_t GetID() const { return id_; }
	MapChipField::Rect GetRect() const;

	bool IsCollidingWithPlayer(const Player* player) const;

private:
	/// 演出 ///
	enum class Behavior {
		kClosed,
		kOpening,
		kOpen,
		kClosing,
	};

	Behavior behavior_ = Behavior::kClosed;
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	bool desiredOpen_ = false;
	float behaviorTimer_ = 0.0f;

	KamataEngine::Vector3 closedPosition_{};
	KamataEngine::Vector3 openPosition_{};

	static inline const float kAnimationDuration = 0.4f;
	static inline const float kOpenDistance = 1.5f;

	void UpdateBehavior();

	void BehaviorClosedInitialize();
	void BehaviorClosedUpdate();

	void BehaviorOpeningInitialize();
	void BehaviorOpeningUpdate();

	void BehaviorOpenInitialize();
	void BehaviorOpenUpdate();

	void BehaviorClosingInitialize();
	void BehaviorClosingUpdate();

	/// ///

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	uint8_t id_ = 0;
	bool isOpen_ = false;
	static inline const KamataEngine::Vector4 kClosedColor = {0.75f, 0.08f, 0.12f, 1.0f};
};
