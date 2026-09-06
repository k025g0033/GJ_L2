#pragma once

///// ----- インクルード ----- /////
#include "KamataEngine.h"

/// <summary>
/// マウスカーソル位置に円を表示する
/// </summary>
class MouseCursor {
public:
	///// ----- 基本処理 ----- /////
	// 初期化
	void Initialize(KamataEngine::Camera* camera);
	// マウス座標とクリック状態を更新する
	void Update();
	// 緑色の円を描画する
	void Draw();

	///// ----- ゲッター ----- /////
	// 左クリックした瞬間か取得する
	bool GetIsClicked() const { return isClicked_; }
	// カーソルのワールド座標を取得する（投げる方向の計算などに使用）
	const KamataEngine::Vector3& GetWorldPosition() const { return worldPosition_; }

private:
	///// ----- 座標変換 ----- /////
	// ウィンドウ座標を3D空間のカーソル座標へ変換する
	KamataEngine::Vector3 ConvertScreenToWorld(const KamataEngine::Vector2& screenPosition) const;

	///// ----- メンバ変数 ----- /////
	// ゲームカメラ
	KamataEngine::Camera* camera_ = nullptr;
	// ウィンドウ上のマウス座標
	KamataEngine::Vector2 screenPosition_ = {};
	// 3D空間へ変換した座標
	KamataEngine::Vector3 worldPosition_ = {};
	// 左クリックした瞬間だけtrue
	bool isClicked_ = false;

	///// ----- 定数 ----- /////
	// カーソル円を表示する奥行き（自機のZ座標に合わせて調整する）
	static inline const float kCursorPlaneZ = 0.0f;
	// 3D空間での円の半径
	static inline const float kRadius = 0.35f;
	// 円を滑らかに見せる線分数
	static inline const uint32_t kSegmentCount = 48;
};