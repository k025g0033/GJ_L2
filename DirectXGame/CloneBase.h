#pragma once
#include "KamataEngine.h"
#include "MapChipField.h"
#include "Player.h"

#include <array>
#include <vector>
#include <optional>
#include <array>

/// <summary>
/// クローンの素
/// マップチップCSV上の "C0" で配置される。
/// 通常は球体（素の状態）で存在し、線がつながると自機と同じ形のクローンに変形する。
/// </summary>
class CloneBase {
public:
	~CloneBase();
	// クローンの素の状態
	enum class State {
		kBase,         // 素の状態（まだ何ともつながっていない）
		kTransforming, // 素 → クローンへの変形アニメーション中
		kTransformed,  // 線がつながり、自機と同じ形に変形した状態
		kReverting,    // クローン → 素へ戻るアニメーション中
	};

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="modelBase">素の状態で使うモデル（球体）</param>
	/// <param name="modelClone">変形後に使うモデル（自機と同じもの）</param>
	/// <param name="camera">カメラ</param>
	/// <param name="position">配置座標</param>
	void Initialize(
	    KamataEngine::Model* modelBase, KamataEngine::Model* modelClone, KamataEngine::Camera* camera,
	    MapChipField* mapChipField, const KamataEngine::Vector3& position);

	// 更新
	void Update(
	    bool isControlled, const std::vector<MapChipField::Rect>& obstacleRects, const MapChipField::Rect& playerRect,
	    const std::vector<MapChipField::Rect>& oneWayPlatformRects = {});


	// 描画
	void Draw();

	///// ----- 変形（素 <-> クローン） ----- /////
	// 線がつながり、自機と同じ形のクローンに変形させる
	// 変形前（素の状態）の現在位置をそのまま引き継ぎ、変形アニメーションを開始する
	void Transform();

	// 素の状態に戻す（自機とのリンクを切った時、またはデバッグ用）
	// 変形中の現在位置をそのまま引き継ぎ、素に戻るアニメーションを開始する。
	// アニメーションが終わってから落下を再開するので、空中で切った場合は自然に落ち、
	// 地上で切った場合は着地判定でその場に留まる
	void ResetToBase();

	// 変形アニメーションの再生中か
	// この間は操作を受け付けず、その場で見た目だけが変化する
	bool IsAnimating() const { return state_ == State::kTransforming || state_ == State::kReverting; }

	// 状態を取得
	State GetState() const { return state_; }

	///// ----- クローンの見た目 ----- /////
	// クローン（変形後）の見た目に使うパーツモデルを設定する（Initialize()の後に呼ぶこと）
	// いったんは自機と同じ頭・左腕・右腕を渡しておき、クローン専用モデルができたら
	// ここへ渡すモデルを差し替えるだけで見た目が切り替わる
	void SetClonePartModels(const std::vector<KamataEngine::Model*>& models) { player_->SetExtraPartModels(models); }

	// クローン（変形後）の本来の表示スケール（当たり判定サイズには影響しない）
	void SetCloneModelScale(float scale) {
		cloneModelScale_ = scale;
		cloneDrawScale_ = scale;
		player_->SetModelScale(scale);
	}

	// ワールドトランスフォームを取得
	const KamataEngine::WorldTransform& GetWorldTransform() const {
		return state_ == State::kTransformed ? player_->GetWorldTransform() : worldTransform_;
	}
	Player* GetPlayer() const { return player_; }

	// 座標を直接設定する（プレイヤーが持っている間、追従させるために使用）
	void SetTranslation(const KamataEngine::Vector3& position) { worldTransform_.translation_ = position; }

	// 持たれている間、目標位置へ向けて移動する（ブロックにめり込まないよう当たり判定で移動量を制限する）
	void MoveHeldTo(const KamataEngine::Vector3& targetPosition);

	///// ----- 持つ・投げる（仮実装） ----- /////
	// プレイヤーに持たれているか
	bool IsHeld() const { return isHeld_; }
	// 持たれた状態にする（投げた後の物理も止める）
	void PickUp() {
		isHeld_ = true;
		isThrown_ = false;
		throwVelocity_ = {};
	}
	// 持たれていない状態に戻す（投げた/離した時）
	void Release() { isHeld_ = false; }
	// 指定した初速で投げる（放物線運動を開始する）
	void Throw(const KamataEngine::Vector3& velocity);
	// 投げられて（重力が働いて）いる最中か
	bool IsThrown() const { return isThrown_; }

	// 当たり判定に使う球の半径（見た目のスケール(kBaseScale)に合わせた値）
	static inline const float kCollisionRadius = 0.5f;

	// 当たり判定サイズの取得
	float GetWidth() const { return kWidth; }
	float GetHeight() const { return kHeight; }

	// 現在位置をもとにした当たり判定用の矩形を取得する（自機との当たり判定で使用）
	MapChipField::Rect GetRect() const;

	// 指定座標に置いたとき、ブロックと重なるかどうかを判定する
	bool IsCollidingWithBlock(const KamataEngine::Vector3& position, MapChipField* mapChipField) const;

	// 消滅通知
	bool ConsumeWaterDestroyed();

	///// ----- 帯電 ----- /////
	// 帯電させる（この周回で一度帯電した記録も同時に残す）
	void Charge();
	void Discharge();
	bool IsCharged() const { return isCharged_; }

	// 今から帯電できるか
	// 「まだ帯電しておらず」かつ「この周回でまだ一度も帯電していない」場合だけ受け取れる。
	// ※これがないと、帯電した相手と接触しっぱなしの間ずっと電気を往復させて
	// 　時間切れを無限に先延ばしできてしまう
	bool CanBeCharged() const { return !isCharged_ && !hasBeenCharged_; }

	// この周回で一度でも帯電したか
	bool HasBeenCharged() const { return hasBeenCharged_; }

	// 帯電履歴をリセットする（全員が帯電を終えた時に、GameScene側からまとめて呼ぶ）
	void ResetChargeHistory() { hasBeenCharged_ = false; }

	float GetChargeRemainingSeconds() const { return static_cast<float>(chargeTimer_) / 60.0f; }
	static float& GetChargeDurationSecondsRef() { return chargeDurationSeconds_; }
	static float GetChargeDurationSeconds() { return chargeDurationSeconds_; }

private:

	///// 演出 /////
	enum class Behavior {
		kNormal,
		kCharging,
		kDischarge,
	};

	Behavior behavior_ = Behavior::kNormal;
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	void UpdateBehavior();

	void BehaviorNormalInitialize();
	void BehaviorNormalUpdate();

	void BehaviorChargingInitialize();
	void BehaviorChargingUpdate();

	void BehaviorDischargeInitialize();
	void BehaviorDischargeUpdate();

	float behaviorTimer_ = 0.0f;
	float chargeEffectTime_ = 0.0f;

	static inline const float kDischargeEffectDuration = 0.3f;

	///// /////

	// 角
	enum Corner {
		kRightBottom, // 右下
		kLeftBottom,  // 左下
		kRightTop,    // 右上
		kLeftTop,     // 左上

		kNumCorner // 要素数
	};

	// ブロックとの当たり判定の結果（上下左右）
	struct BlockCollisionResult {
		bool isCeilingHit = false; // 天井との当たり判定
		bool isGroundHit = false;  // 地面との当たり判定
		bool isWallHit = false;    // 壁との当たり判定
	};

	// 投げられて飛んでいる間の物理更新（重力・着地判定）
	void UpdateThrowPhysics(
	    const MapChipField::Rect& playerRect,
	    const std::vector<MapChipField::Rect>& oneWayPlatformRects);

	///// ----- 変形アニメーション ----- /////
	// 素 <-> クローンの変形アニメーションを1フレーム分進める
	void UpdateTransformAnimation();

	///// ----- ブロックとの当たり判定（Player::isMapCollision系を参考に実装） ----- /////
	// 指定した移動量に対して、上下左右のブロック衝突をまとめて判定し、めり込まないよう移動量を補正する
	// ※X・Y各方向を独立して判定するため、斜め移動でもブロックへのめり込み・すり抜けが起きない
	BlockCollisionResult CheckBlockCollision(KamataEngine::Vector3& moveAmount) const;
	void CheckBlockCollisionTop(KamataEngine::Vector3& moveAmount, BlockCollisionResult& result) const;
	void CheckBlockCollisionBottom(KamataEngine::Vector3& moveAmount, BlockCollisionResult& result) const;
	void CheckBlockCollisionRight(KamataEngine::Vector3& moveAmount, BlockCollisionResult& result) const;
	void CheckBlockCollisionLeft(KamataEngine::Vector3& moveAmount, BlockCollisionResult& result) const;

	// 指定した中心座標から見た、指定した角の座標を計算する
	KamataEngine::Vector3 CornerPosition(const KamataEngine::Vector3& center, Corner corner) const;
	// 指定した移動量だけ進んだ場合の4つの角の座標をまとめて計算して返す
	std::array<KamataEngine::Vector3, kNumCorner> GetCalculatedCorners(const KamataEngine::Vector3& moveAmount) const;

	// ワールド変換データ
	KamataEngine::WorldTransform worldTransform_;

	// 素の状態のモデル（球体）
	KamataEngine::Model* modelBase_ = nullptr;
	// 変形後のモデル（自機と同じもの）
	KamataEngine::Model* modelClone_ = nullptr;
	// カメラ
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;

	// 初期位置を保存
	KamataEngine::Vector3 initialPosition_ = {};

	// 色
	KamataEngine::ObjectColor chargeColor_;

	// 消滅フラグ
	bool wasDestroyedByWater_ = false;

	// 帯電フラグ
	bool isCharged_ = false;
	// 帯電時間
	int chargeTimer_ = 0;
	// 全クローン共通の帯電時間（秒）。ImGuiから変更する。
	static inline float chargeDurationSeconds_ = 5.0f;
	static inline const float kFramesPerSecond = 60.0f;


	// 現在の状態
	State state_ = State::kBase;

	// マップチップフィールド（投げた後の着地判定に使用）
	MapChipField* mapChipField_ = nullptr;

	// プレイヤーに持たれているか（仮実装）
	bool isHeld_ = false;

	// 素の状態での表示スケール（球体モデルを1マスに収める）
	static inline const float kBaseScale = 0.5f;

	///// ----- 当たり判定(立方体) ----- /////
	// 見た目は仮で球体だが、当たり判定は自機と同じく立方体として扱う
	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;
	// ブロックにめり込まないための微小な余白（Playerのkblankと同じ考え方）
	static inline const float kBlank = 0.02f;

	///// ----- 投げる処理 ----- /////
	// 投げられて飛んでいる間の速度
	KamataEngine::Vector3 throwVelocity_ = {};
	// 投げられて（重力が働いて）いる状態か
	bool isThrown_ = false;
	// 投げた後にかかる重力加速度
	static inline const float kThrowGravity = 0.02f;
	// 落下速度の上限
	static inline const float kThrowMaxFallSpeed = 0.5f;
	// 着地判定のすき間（誤差吸収用）
	static inline const float kLandingBlank = 0.02f;

	///// ----- 帯電 ----- /////
	// この周回で一度でも帯電したか（一度使ったクローンは、全員が使い終わるまで再帯電できない）
	bool hasBeenCharged_ = false;

	///// ----- 変形アニメーション ----- /////
	// アニメーションの経過フレーム数
	int animationTimer_ = 0;
	// アニメーション全体のフレーム数
	int animationDuration_ = 0;

	// クローンの現在の表示スケール（アニメーション中に0から本来の大きさまで変化する）
	float cloneDrawScale_ = 1.5f;
	// クローンの本来の表示スケール（自機と同じ大きさに合わせる。SetCloneModelScaleで変更する）
	float cloneModelScale_ = 1.5f;

	/// --- アニメーションにかかる時間 ---
	// 素 → クローン
	static inline const float kTransformSeconds = 1.0f;
	// クローン → 素
	static inline const float kRevertSeconds = 0.5f;

	/// --- 素 → クローン の内訳（合計が1.0になるようにする） ---
	// 球体がスライムのように伸び縮みする区間
	static inline const float kTransformSquashRatio = 0.55f;
	// 球体が縮んで消える区間
	static inline const float kTransformShrinkRatio = 0.20f;
	// クローンが0から現れる区間
	static inline const float kTransformGrowRatio = 0.25f;

	/// --- クローン → 素 の内訳（合計が1.0になるようにする） ---
	// クローンが縮んで消える区間
	static inline const float kRevertShrinkRatio = 0.5f;
	// 球体が0から現れる区間
	static inline const float kRevertGrowRatio = 0.5f;

	/// --- 伸び縮みの調整値 ---
	// つぶれる側の強さ（0.45なら、つぶれた時に横が0.55倍・縦が1.45倍になる）
	static inline const float kSquashStrength = 0.7f;
	// 伸びる側の強さ
	// ※つぶれる側より小さくしておくことで、伸びた瞬間でも元の大きさを超えず、
	// 　全体としては常に小さくなっていくように見える
	static inline const float kStretchStrength = 0.3f;
	// 伸び縮みを何往復させるか
	static inline const float kSquashWaveCount = 3.0f;
};
