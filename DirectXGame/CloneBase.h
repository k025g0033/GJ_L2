#pragma once
#include "KamataEngine.h"

/// <summary>
/// クローンの素
/// マップチップCSV上の "C0" で配置される。
/// 通常は球体（素の状態）で存在し、線がつながると自機と同じ形のクローンに変形する。
/// </summary>
class CloneBase {
public:
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
	void Initialize(KamataEngine::Model* modelBase, KamataEngine::Model* modelClone, KamataEngine::Camera* camera, const KamataEngine::Vector3& position);

	// 更新
	void Update();

	// 描画
	void Draw();

	// 線がつながり、自機と同じ形のクローンに変形させる
	void Transform() { state_ = State::kTransformed; }

	// 素の状態に戻す（デバッグ用）
	void ResetToBase() { state_ = State::kBase; }

	// 状態を取得
	State GetState() const { return state_; }

	// ワールドトランスフォームを取得
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }

private:
	// ワールド変換データ
	KamataEngine::WorldTransform worldTransform_;

	// 素の状態のモデル（球体）
	KamataEngine::Model* modelBase_ = nullptr;
	// 変形後のモデル（自機と同じもの）
	KamataEngine::Model* modelClone_ = nullptr;
	// カメラ
	KamataEngine::Camera* camera_ = nullptr;

	// 現在の状態
	State state_ = State::kBase;

	// 素の状態での表示スケール（球体モデルを1マスに収める）
	static inline const float kBaseScale = 0.5f;
};
