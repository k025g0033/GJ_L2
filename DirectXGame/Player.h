#pragma once
#include "KamataEngine.h"

class MapChipField;

class Player {
public:
	// 左右
	enum class LRDirection {
		kRight,
		kLeft,
	};

	// 角
	enum Corner {
		kRightBottom, // 右下
		kLeftBottom,  // 左下
		kRightTop,    // 右上
		kLeftTop,     // 左上

		kNumCorner // 要素数
	};

	struct CollisionMapInfo {
		bool isCeilingCollision = false;    // 天井との当たり判定
		bool isGroundCollision = false;     // 地面との当たり判定
		bool isWallCollision = false;       // 壁との当たり判定
		KamataEngine::Vector3 moveVelocity; // 移動量
	};

	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* cameraz, const KamataEngine::Vector3& position);

	void Update(bool canMove);

	void Draw();

	void Move();

	void isMapCollision(CollisionMapInfo& info);

	void isMapCollisionTop(CollisionMapInfo& info);

	void isMapCollisionBottom(CollisionMapInfo& info);

	void isMapCollisionRight(CollisionMapInfo& info);

	void isMapCollisionLeft(CollisionMapInfo& info);

	void isCollisionMove(const CollisionMapInfo& info);

	void isHitCeiling(const CollisionMapInfo& info);

	void isOnGround(const CollisionMapInfo& info);

	void isHitWall(const CollisionMapInfo& info);

	KamataEngine::Vector3 CornerPosition(const KamataEngine::Vector3& center, Corner corner);

	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }

	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }

	bool IsOnGround() const { return onGround_; }

	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	// 当たり判定サイズの取得（クローンの素との当たり判定などで使用）
	float GetWidth() const { return kWidth; }
	float GetHeight() const { return kHeight; }

	// 現在向いている方向を取得（クローンの素をどちら側に持つか判定するのに使用）
	LRDirection GetLRDirection() const { return lrDirection_; }

	// 旋回を強制的にキャンセルし、逆方向へ戻す
	// （方向転換先にブロックがあり、向けない時に使用）
	void CancelTurn();

private:
	// マップチップによるフィールド
	MapChipField* mapChipField_ = nullptr;

	// ワールド変換データ
	KamataEngine::WorldTransform worldTransform_;
	// モデル
	KamataEngine::Model* model_ = nullptr;

	// カメラ
	KamataEngine::Camera* camera_ = nullptr;

	KamataEngine::Vector3 velocity_ = {};

	// プレイヤーの速度
	static inline const float kLimitRunSpeed = 0.15f;

	LRDirection lrDirection_ = LRDirection::kRight;

	// 旋回開始時の角度
	float turnFirstRotationY_ = 0.0f;
	// 旋回タイマー
	float turnTimer_ = 0.0f;

	// 旋回時間<秒>
	static inline const float kTimeTurn = 0.21f;

	// 接地状態フラグ
	bool onGround_ = true;

	// 重力加速度 (下方向)
	static inline const float kGravityAcceleration = 0.02f;
	// 最大落下速度 (下方向)
	static inline const float kLimitFallSpeed = 0.5f;
	// ジャンプ初速 (上方向)
	static inline const float kJumpAcceleration = 0.25f;

	// キャラクターの当たり判定サイズ
	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;

	static inline const float kBlank = 0.02f;

	// 着地時の速度減衰率
	static inline const float kAttenuationLanding = 0.5f;

	// 地面吸着判定で下方向にずらす距離
	static inline const float kGroundSearchHeight = 0.05f;

	// 着地時の速度減衰率
	static inline const float kAttenuationWall = 0.5f;
};
