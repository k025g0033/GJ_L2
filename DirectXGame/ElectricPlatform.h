#pragma once

#include "KamataEngine.h"
#include "MapChipField.h"
#include <cstdint>

// 帯電1回ごとに一定距離進み、最後の移動完了から一定時間後に始点へ戻る足場
class ElectricPlatform {
public:
	void Initialize(
	    KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& start,
	    const KamataEngine::Vector3& end, uint8_t id);
	void Update();
	void Draw();

	void Charge();
	// 他の電動足場と衝突した時などに、待機せず初期位置への帰還を始める。
	void ReturnToStart();
	bool IsCharged() const { return isMovingForward_ || returnWaitTimer_ > 0; }
	uint8_t GetID() const { return id_; }
	MapChipField::Rect GetRect() const;
	const KamataEngine::Vector3& GetMoveDelta() const { return moveDelta_; }

private:

	/// 演出 ///
	enum class Behavior {
		kIdle,      // 初期位置で待機
		kMoving,    // 通電して前進
		kWaiting,   // 移動後、帰還まで待機
		kReturning, // 初期位置へ帰還
	};

	Behavior behavior_ = Behavior::kIdle;
	// 演出用
	float effectTimer_ = 0.0f;
	static inline const KamataEngine::Vector4 kWaitingColor = {0.3f, 0.9f, 1.0f, 1.0f};
	static inline const KamataEngine::Vector4 kReturningColor = {0.2f, 0.45f, 0.55f, 1.0f};
	static inline const int kReturnWarningFrames = 30;

	/// ///

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;

	KamataEngine::Vector3 start_{};
	KamataEngine::Vector3 moveStep_{};
	KamataEngine::Vector3 forwardTarget_{};
	KamataEngine::Vector3 moveDelta_{};
	uint8_t id_ = 0;
	int returnWaitTimer_ = 0;
	bool isMovingForward_ = false;
	bool isReturning_ = false;

	// ElecPlate.obj（幅約2.16、高さ約0.82）を0.46倍で表示した大きさに合わせる。
	static inline const float kModelScale = 0.46f;
	static inline const float kWidth = 1.0f;
	static inline const float kHeight = 0.38f;
	static inline const float kMoveSpeed = 0.04f;
	static inline const int kReturnWaitFrames = 60 * 2;
	static inline const KamataEngine::Vector4 kIdleColor = {0.25f, 0.3f, 0.35f, 1.0f};
	static inline const KamataEngine::Vector4 kChargedColor = {0.15f, 0.85f, 1.0f, 1.0f};
};
