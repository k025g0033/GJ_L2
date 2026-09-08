#pragma once
#include "KamataEngine.h"
#include "MapChipField.h" // 前方宣言から実体includeに変更（Rectを使うため）

#include <vector>

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

	void Update(
	    bool canMove, const std::vector<MapChipField::Rect>& obstacleRects,
	    const std::vector<MapChipField::Rect>& oneWayPlatformRects = {});

	void Draw(KamataEngine::ObjectColor* objectColor = nullptr);

	void Move();

	///// ----- 自機とブロックの当たり判定 ----- /////
	void isMapCollision(CollisionMapInfo& info);
	void isMapCollisionTop(CollisionMapInfo& info);
	void isMapCollisionBottom(CollisionMapInfo& info);
	void isMapCollisionRight(CollisionMapInfo& info);
	void isMapCollisionLeft(CollisionMapInfo& info);

	///// ----- クローンの素などの、マップチップ以外の障害物との当たり判定 ----- /////
	void isObstacleCollision(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects);
	void isObstacleCollisionTop(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects);
	void isObstacleCollisionBottom(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects);
	void isObstacleCollisionRight(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects);
	void isObstacleCollisionLeft(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects);

	void isCollisionMove(const CollisionMapInfo& info);

	void isHitCeiling(const CollisionMapInfo& info);

	// 障害物の矩形一覧も見て、乗っているか判定するように変更
	void isOnGround(const CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects);

	void isHitWall(const CollisionMapInfo& info);

	KamataEngine::Vector3 CornerPosition(const KamataEngine::Vector3& center, Corner corner);

	// 中心から見た左右それぞれの半幅を取得する
	// 通常時は左右とも kWidth/2 で対称だが、持っている間は「背中側」は kWidth/2 のまま固定し、
	// 「向いている方向側」だけを GetWidth() まで伸ばす（＝広がった分は全部前方向に付く）
	float GetLeftHalfWidth() const;
	float GetRightHalfWidth() const;

	// 指定した移動量だけ進んだ場合の4つの角の座標をまとめて計算して返す（共通化関数）
	// ※斜め移動時にX・Y各方向の当たり判定が影響し合わないよう、各判定関数の中で
	//   実際に使うのは判定対象の軸の移動量だけになるよう別途計算しなおしている
	std::array<KamataEngine::Vector3, kNumCorner> GetCalculatedCorners(const KamataEngine::Vector3& moveAmount);

	bool IsOnGround() const { return onGround_; }

	// 旋回を強制的にキャンセルし、逆方向へ戻す
	// （方向転換先にブロックがあり、向けない時に使用）
	void CancelTurn();

	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }

	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }

	// 当たり判定サイズの取得
	// ※クローンの素を持っていても当たり判定のサイズは変わらない（常に通常時と同じ）
	float GetWidth() const { return kWidth; }
	float GetHeight() const { return kHeight; }

	// 持った状態に関係なく、常に通常時の横幅を返す（クローンの素の追従位置計算などで使用）
	static float GetNormalWidth() { return kWidth; }

	// 当たり判定サイズ（ImGuiのスライダーから直接書き換えられるよう参照を返す）
	// ※自機とクローンで共通の値なので、片方を変えるともう片方にも反映される
	static float& GetWidthRef() { return kWidth; }
	static float& GetHeightRef() { return kHeight; }

	// クローンの素を持っている状態かどうかを設定する
	// （当たり判定のサイズは変えず、見た目のモデルだけを持っている用に差し替える）
	void SetIsHolding(bool isHolding) { isHolding_ = isHolding; }
	bool IsHolding() const { return isHolding_; }

	// ジャンプ可能かどうかを設定する（クローンはジャンプできないようにするために使用）
	void SetCanJump(bool canJump) { canJump_ = canJump; }
	bool GetCanJump() const { return canJump_; }

	// 持っている間だけ使うベースモデルを設定する（未設定ならmodel_をそのまま使う）
	void SetHoldingModel(KamataEngine::Model* model) { holdingModel_ = model; }

	// クローンの素を持っている間だけ使う追加パーツを設定する
	// （腕を上げた形のモデルと、抱えているクローンのモデルをまとめて渡す想定）
	// 未設定の場合は、持っている間もSetExtraPartModelsで渡した通常のパーツをそのまま使う。
	void SetHoldingPartModels(const std::vector<KamataEngine::Model*>& models) { holdingPartModels_ = models; }

	// 体のベースモデルに重ねて描画する追加パーツ（頭・腕など）を設定する
	// ※Blender側で原点をワールド原点に合わせてエクスポートしてあるので、
	// 　ベースモデルと同じワールド変換で描画するだけで正しい位置に組み合わさる。
	// 　クローン側には設定しないので、クローンは今まで通りベースモデルだけで描画される。
	void SetExtraPartModels(const std::vector<KamataEngine::Model*>& models) { extraPartModels_ = models; }

	// モデルの表示スケール（見た目の大きさだけを変える。当たり判定サイズ(kWidth/kHeight)には影響しない）
	void SetModelScale(float scale) { modelScale_ = scale; }
	float GetModelScale() const { return modelScale_; }
	// ImGuiのスライダーから直接書き換えられるよう参照を返す
	float& GetModelScaleRef() { return modelScale_; }

	// モデルの表示スケールを即座に反映する（Update()を呼ばずに見た目だけを変えたい時に使用）
	// クローンの変形アニメーションのように、動かさずにスケールだけ変化させる場合に使う
	void SetModelScaleImmediate(float scale);

	// 現在向いている方向を取得（クローンの素をどちら側に持つか判定するのに使用）
	LRDirection GetLRDirection() const { return lrDirection_; }

	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	// 座標を直接設定する（クローンの素との当たり判定で押し出す時に使用）
	void SetTranslation(const KamataEngine::Vector3& position) { worldTransform_.translation_ = position; }

	// 水中状態
	bool IsInWater() const { return isInWater_; }
	// リスポーン機能
	void Respawn(const KamataEngine::Vector3& position);

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

	// キャラクターの当たり判定サイズ（ImGuiで調整できるようconstにしていない）
	// ※自機とクローンで共通の値。横幅は0.8だと少し広く感じたので0.7に狭めてある。
	static inline float kWidth = 0.7f;
	static inline float kHeight = 0.8f;

	static inline const float kBlank = 0.02f;

	// クローンの素を持っているか（当たり判定は変えず、見た目のモデルだけ差し替える）
	bool isHolding_ = false;

	// ジャンプできるか（通常の自機はtrue、クローンはfalseにする）
	bool canJump_ = true;

	// 持っている間だけ使うモデル（未設定ならmodel_をそのまま使う）
	KamataEngine::Model* holdingModel_ = nullptr;

	// ベースモデルに重ねて描画する追加パーツ（頭・腕など）。未設定なら何も追加描画しない。
	std::vector<KamataEngine::Model*> extraPartModels_;

	// クローンの素を持っている間だけ使う追加パーツ。未設定ならextraPartModels_をそのまま使う。
	std::vector<KamataEngine::Model*> holdingPartModels_;

	// モデルの表示スケール（当たり判定サイズには影響せず、見た目の大きさだけを変える）
	float modelScale_ = 1.0f;

	// 着地時の速度減衰率
	static inline const float kAttenuationLanding = 0.5f;

	// 地面吸着判定で下方向にずらす距離
	static inline const float kGroundSearchHeight = 0.05f;

	// 着地時の速度減衰率
	static inline const float kAttenuationWall = 0.5f;

	// 水に入っているか
	bool isInWater_ = false;
	bool wasInWater_ = false;
	uint32_t waterExitGraceFrames_ = 0;
	void CheckInWater();
	void MoveInWater();

	// 操作を受け付けない間の落下処理（重力だけを働かせる）
	void ApplyGravityOnly();

	// 水中時の数値
	static inline const float kSwimSpeedX = 0.08f;      // X方向速度
	static inline const float kSwimSpeedY = 0.08f;      // Y方向速度
	static inline const float kWaterGravity = 0.003f;   // 重力
	static inline const float kWaterResistance = 0.05f; // 抵抗
	static inline const float kLimitWaterFallSpeed = 0.04f; // 水中の最大下降速度
	static inline const float kWaterExitJumpSpeed = 0.22f;  // 水面から飛び出す初速
	static inline const uint32_t kWaterExitGraceFrameCount = 8; // 離水後にジャンプを受け付ける猶予
};
