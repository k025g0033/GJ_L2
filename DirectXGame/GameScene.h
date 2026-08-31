#pragma once
#include "KamataEngine.h"

using namespace KamataEngine;

// ゲームシーン
class GameScene {
public:
	// デストラクタ
	~GameScene();

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

private:
	// テクスチャハンドル
	uint32_t textureHandle_ = 0;
	// スプライト
	Sprite* sprite_ = nullptr;
};
