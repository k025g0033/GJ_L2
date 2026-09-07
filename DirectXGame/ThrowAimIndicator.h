#pragma once

///// ----- インクルード ----- /////
#include "KamataEngine.h"

/// <summary>
/// クローンの素を持っている間に表示する、投げる方向を示すUI。
/// 自機を中心としたワイヤーフレームの円と、その円周上を回る三角形（画像）で構成される。
/// 三角形のとがっている方向が、そのままクローンの素を投げる方向になる。
/// </summary>
class ThrowAimIndicator {
public:
	///// ----- 基本処理 ----- /////
	// 初期化（三角形の画像読み込みなど）
	void Initialize(KamataEngine::Camera* camera);

	// 自機とマウスカーソルのワールド座標から、狙う方向・三角形の位置と回転を計算する
	// isHoldingCloneBase_ がtrueの間だけGameScene側から呼び出す想定
	void Update(const KamataEngine::Vector3& playerPosition, const KamataEngine::Vector3& mouseWorldPosition);

	// 円（ワイヤーフレーム）と三角形（画像）を描画する
	void Draw();

	///// ----- ゲッター ----- /////
	// 投げる方向（正規化済み、Z成分は0）を取得する
	const KamataEngine::Vector3& GetThrowDirection() const { return throwDirection_; }

	///// ----- ImGuiから調整するパラメータ ----- /////
	// 自機を中心とした円の半径（ワールド単位）
	float circleRadius_ = 1.7f;
	// 三角形（画像）の表示サイズ（ピクセル）
	float triangleSize_ = 45.0f;

private:
	// ワールド座標をスクリーン座標へ変換する（MouseCursor::ConvertScreenToWorldの逆変換）
	KamataEngine::Vector2 ConvertWorldToScreen(const KamataEngine::Vector3& worldPosition) const;

	///// ----- メンバ変数 ----- /////
	// カメラ
	KamataEngine::Camera* camera_ = nullptr;
	// 円周上を回る三角形の画像（Resources/images/cursor.png）
	KamataEngine::Sprite* triangleSprite_ = nullptr;
	uint32_t triangleTextureHandle_ = 0;

	// 自機の現在位置（円の中心）
	KamataEngine::Vector3 playerPosition_ = {};
	// 投げる方向（正規化済み、Z成分は0。初期値は右向き）
	KamataEngine::Vector3 throwDirection_ = {1.0f, 0.0f, 0.0f};

	///// ----- 定数 ----- /////
	// 円を滑らかに見せる線分数
	static inline const uint32_t kSegmentCount = 48;
};
