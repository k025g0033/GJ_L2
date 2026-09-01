#pragma once
#include "KamataEngine.h"
#include "Player.h"

class Player;

class CameraController {
public:
	// 矩形
	struct Rect {
		float left = 0.0f;
		float right = 1.0f;
		float bottom = 0.0f;
		float top = 1.0f;
	};

	void Initialize(KamataEngine::Camera* camera);

	void Update();

	void SetTarget(Player* target) { target_ = target; }

	void Reset();

	void SetMovableArea(Rect area) { movableArea_ = area; }

private:
	// ワールド変換データ
	KamataEngine::WorldTransform worldTransform_;
	// カメラ
	KamataEngine::Camera* camera_ = nullptr;

	Player* target_ = nullptr;

	// 追従対象とカメラの座標の差(オフセット)
	KamataEngine::Vector3 targetOffset_ = {0.0f, 4.5f, -15.0f};

	// カメラ移動範囲
	Rect movableArea_ = {0, 100, 0, 100};

	// カメラの目標座標
	KamataEngine::Vector3 targetPos_;

	// 座標補間割合
	static inline const float kInterpolationRate = 0.3f;

	// 速度掛け算
	static inline const float kVelocityBias = 5.0f;

	// 追従対象の各方向へのカメラ移動範囲
	static inline const Rect margin = {-5.0f, 5.0f, -3.0f, 3.0f};
};