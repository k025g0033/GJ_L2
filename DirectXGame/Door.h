#pragma once

#include "KamataEngine.h"
#include "MapChipField.h"
#include <cstdint>

// 同じIDの感圧板から開閉状態を受け取る扉
class Door {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position, uint8_t id);
	void Update();
	void Draw();

	void SetOpen(bool isOpen) { isOpen_ = isOpen; }
	bool IsOpen() const { return isOpen_; }
	uint8_t GetID() const { return id_; }
	MapChipField::Rect GetRect() const;

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	uint8_t id_ = 0;
	bool isOpen_ = false;
	static inline const KamataEngine::Vector4 kClosedColor = {0.75f, 0.08f, 0.12f, 1.0f};
};
