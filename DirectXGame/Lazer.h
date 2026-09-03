#pragma once
#include "KamataEngine.h"

class Lazer {
public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& start, const KamataEngine::Vector3& end);

	void Update();
	void Draw();

private:
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	KamataEngine::WorldTransform worldTransform_;
};
