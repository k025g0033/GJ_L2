#pragma once
#include "KamataEngine.h"
#include "Player.h"

// 前方宣言
class MapChipField;

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
		kBase,        // 素の状態（まだ何ともつながっていない）
		kTransformed, // 線がつながり、自機と同じ形に変形した状態
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
	void Update(bool isControlled);

	// 描画
	void Draw();

	// 線がつながり、自機と同じ形のクローンに変形させる
	void Transform() { state_ = State::kTransformed; }

	// 素の状態に戻す（デバッグ用）
	void ResetToBase() { state_ = State::kBase; }

	// 状態を取得
	State GetState() const { return state_; }

	// ワールドトランスフォームを取得
	const KamataEngine::WorldTransform& GetWorldTransform() const {
		return state_ == State::kTransformed ? player_->GetWorldTransform() : worldTransform_;
	}
	Player* GetPlayer() const { return player_; }

	// 座標を直接設定する（プレイヤーが持っている間、追従させるために使用）
	void SetTranslation(const KamataEngine::Vector3& position) { worldTransform_.translation_ = position; }

	///// ----- 持つ・投げる（仮実装） ----- /////
	// プレイヤーに持たれているか
	bool IsHeld() const { return isHeld_; }
	// 持たれた状態にする
	void PickUp() { isHeld_ = true; }
	// 持たれていない状態に戻す（投げた/離した時）
	void Release() { isHeld_ = false; }

	// 当たり判定に使う球の半径（見た目のスケール(kBaseScale)に合わせた値）
	static inline const float kCollisionRadius = 0.5f;

	// 当たり判定サイズの取得
	float GetWidth() const { return kWidth; }
	float GetHeight() const { return kHeight; }

	// 指定座標に置いたとき、ブロックと重なるかどうかを判定する
	bool IsCollidingWithBlock(const KamataEngine::Vector3& position, MapChipField* mapChipField) const;

private:
	// ワールド変換データ
	KamataEngine::WorldTransform worldTransform_;

	// 素の状態のモデル（球体）
	KamataEngine::Model* modelBase_ = nullptr;
	// 変形後のモデル（自機と同じもの）
	KamataEngine::Model* modelClone_ = nullptr;
	// カメラ
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;

	// 現在の状態
	State state_ = State::kBase;

	// プレイヤーに持たれているか（仮実装）
	bool isHeld_ = false;

	// 素の状態での表示スケール（球体モデルを1マスに収める）
	static inline const float kBaseScale = 0.5f;

	///// ----- 当たり判定(立方体) ----- /////
	// 見た目は仮で球体だが、当たり判定は自機と同じく立方体として扱う
	static inline const float kWidth = 0.8f;
	static inline const float kHeight = 0.8f;
};
