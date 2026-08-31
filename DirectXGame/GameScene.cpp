#include "GameScene.h"

using namespace KamataEngine;

GameScene::~GameScene() {
	// スプライトの解放
	delete sprite_;
}

void GameScene::Initialize() {
	// テクスチャ読み込み(Resources/uvChecker.png)
	textureHandle_ = TextureManager::Load("uvChecker.png");

	// スプライト生成(左上寄りの座標に表示)
	sprite_ = Sprite::Create(textureHandle_, {100.0f, 100.0f});
}

void GameScene::Update() {}

void GameScene::Draw() {
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// スプライト描画前処理
	Sprite::PreDraw(dxCommon->GetCommandList());

	// スプライトの描画
	sprite_->Draw();

	// スプライト描画後処理
	Sprite::PostDraw();
}
