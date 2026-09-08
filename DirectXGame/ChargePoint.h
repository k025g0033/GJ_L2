#pragma once
#include "KamataEngine.h"
#include "MapChipField.h"

class ChargePoint {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position);

	void Update();
	void Draw();

	MapChipField::Rect GetRect() const;

private:
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;

	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;
};