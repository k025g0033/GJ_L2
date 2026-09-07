#include "TitleScene.h"
#include "KamataEngine.h"
#include "WorldTransformConfig.h"
#include <Windows.h>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

TitleScene::~TitleScene() {
	delete titleModel_;
	delete startSprite_;
	delete exitSprite_;
}

void TitleScene::Initialize() {
	isFinished_ = false;
	selectedItem_ = MenuItem::kStart;
	selectionAnimationTime_ = 0.0f;

	startTextureHandle_ = TextureManager::Load("Title/Start.png");
	exitTextureHandle_ = TextureManager::Load("Title/Exit.png");
	startSprite_ = Sprite::Create(startTextureHandle_, {576.0f, 360.0f});
	exitSprite_ = Sprite::Create(exitTextureHandle_, {576.0f, 480.0f});

	titleModel_ = Model::CreateFromOBJ("TitleLogo", true);
	titleWorldTransform_.Initialize();
	titleWorldTransform_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	titleWorldTransform_.rotation_.y = std::numbers::pi_v<float>;
	titleWorldTransform_.translation_ = {0.0f, 2.0f, 0.0f};
	UpdateWorldTransform(titleWorldTransform_);

	titleCamera_.Initialize();
	titleCamera_.translation_ = {0.0f, 0.0f, -10.0f};
	titleCamera_.UpdateMatrix();
}

void TitleScene::Update() {
	selectionAnimationTime_ += 1.0f / 60.0f;
	UpdateWorldTransform(titleWorldTransform_);
	titleCamera_.UpdateMatrix();

	if (Input::GetInstance()->TriggerKey(DIK_W) || Input::GetInstance()->TriggerKey(DIK_S)) {
		selectedItem_ = selectedItem_ == MenuItem::kStart ? MenuItem::kExit : MenuItem::kStart;
		selectionAnimationTime_ = 0.0f;
	}

	float pulse = (std::sin(selectionAnimationTime_ * kAnimationSpeed) + 1.0f) * 0.5f;
	float selectedSize = kImageSize * (1.0f + pulse * kAnimationScale);
	float selectedOffset = (selectedSize - kImageSize) * 0.5f;

	if (selectedItem_ == MenuItem::kStart) {
		startSprite_->SetSize({selectedSize, selectedSize});
		startSprite_->SetPosition({576.0f - selectedOffset, 360.0f - selectedOffset});
		exitSprite_->SetSize({kImageSize, kImageSize});
		exitSprite_->SetPosition({576.0f, 480.0f});
	} else {
		startSprite_->SetSize({kImageSize, kImageSize});
		startSprite_->SetPosition({576.0f, 360.0f});
		exitSprite_->SetSize({selectedSize, selectedSize});
		exitSprite_->SetPosition({576.0f - selectedOffset, 480.0f - selectedOffset});
	}

	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		if (selectedItem_ == MenuItem::kStart) {
			isFinished_ = true;
		} else {
			PostQuitMessage(0);
		}
	}
}

void TitleScene::Draw() {
	Model::PreDraw();
	titleModel_->Draw(titleWorldTransform_, titleCamera_);
	Model::PostDraw();

	if (selectedItem_ == MenuItem::kStart) {
		DebugText::GetInstance()->Print(">", 540.0f, 410.0f, 1.5f);
	} else {
		DebugText::GetInstance()->Print(">", 540.0f, 530.0f, 1.5f);
	}

	Sprite::PreDraw();
	startSprite_->Draw();
	exitSprite_->Draw();
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();
}
