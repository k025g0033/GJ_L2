#include "Skydome.h"

void Skydome::Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera) {
	// NILLポインタチェック
	assert(model);

	// 引数の値をメンバ変数にコピー
	model_ = model;
	camera_ = camera;

	// ワールド変換の初期化
	worldTransform_.Initialize();
}

void Skydome::Update() {}

void Skydome::Draw() {
	// 3Dモデルを描画
	model_->Draw(worldTransform_, *camera_);
}