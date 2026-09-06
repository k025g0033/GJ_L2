#pragma once
#include "KamataEngine.h"
#include "MapChipField.h"
#include <cstdint>

class Lazer {
public:
	void Initialize(
	    KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& start,
	    const KamataEngine::Vector3& end, uint8_t id);

	void Update();
	void Draw();

	void SetActive(bool isActive) { isActive_ = isActive; }
	bool IsActive() const { return isActive_; }
	uint8_t GetID() const { return id_; }
	MapChipField::Rect GetRect() const { return collisionRect_; }

private:
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	KamataEngine::WorldTransform worldTransform_;
	MapChipField::Rect collisionRect_{};
	uint8_t id_ = 0;
	bool isActive_ = true;
};
