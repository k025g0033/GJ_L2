#pragma once
#include "KamataEngine.h"
#include "MapChipField.h"
#include <cstdint>
#include <optional>

class Lazer {
public:
	void Initialize(
	    KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& start,
	    const KamataEngine::Vector3& end, uint8_t id);

	void Update();
	void Draw();

	void SetActive(bool isActive);
	bool IsActive() const { return isActive_; }
	uint8_t GetID() const { return id_; }
	MapChipField::Rect GetRect() const { return collisionRect_; }

private:
	/// 演出 ///
	enum class Behavior {
		kActive,
		kDisappearing,
		kInactive,
		kAppearing,
	};

	Behavior behavior_ = Behavior::kActive;
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	bool desiredActive_ = true;
	float behaviorTimer_ = 0.0f;

	KamataEngine::Vector3 baseScale_{};

	static inline const float kAnimationDuration = 0.25f;

	void UpdateBehavior();

	void BehaviorActiveInitialize();
	void BehaviorActiveUpdate();

	void BehaviorDisappearingInitialize();
	void BehaviorDisappearingUpdate();

	void BehaviorInactiveInitialize();
	void BehaviorInactiveUpdate();

	void BehaviorAppearingInitialize();
	void BehaviorAppearingUpdate();

	/// ///

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	KamataEngine::WorldTransform worldTransform_;
	MapChipField::Rect collisionRect_{};
	uint8_t id_ = 0;
	bool isActive_ = true;
	KamataEngine::ObjectColor color_;
	float effectTime_ = 0.0f;
};
