#include "TitleScene.h"
#include "AudioSettings.h"
#include "GameScene.h"
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
	delete titleBackgroundScene_;
}

void TitleScene::Initialize() {
	isFinished_ = false;
	selectedItem_ = MenuItem::kStart;
	selectionAnimationTime_ = 0.0f;
	cursorMoveSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/CursorMove.wav");
	decideSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/Decide.wav");
	titleBackgroundScene_ = new GameScene(1, true);
	titleBackgroundScene_->Initialize();

	startTextureHandle_ = TextureManager::Load("Title/Start.png");
	exitTextureHandle_ = TextureManager::Load("Title/Exit.png");
	startSprite_ = Sprite::Create(startTextureHandle_, {576.0f, 400.0f});
	exitSprite_ = Sprite::Create(exitTextureHandle_, {576.0f, 520.0f});

	titleModel_ = Model::CreateFromOBJ("TitleLogo", true);
	titleWorldTransform_.Initialize();
	titleWorldTransform_.rotation_.x = std::numbers::pi_v<float> / 2.0f;
	titleWorldTransform_.rotation_.y = std::numbers::pi_v<float>;
	titleWorldTransform_.translation_ = {0.0f, 1.4f, 0.0f};
	UpdateWorldTransform(titleWorldTransform_);

	titleCamera_.Initialize();
	titleCamera_.translation_ = {0.0f, 0.0f, -10.0f};
	titleCamera_.UpdateMatrix();
}

void TitleScene::Update() {
	selectionAnimationTime_ += 1.0f / 60.0f;
	titleBackgroundScene_->UpdateTitleBackground();
	UpdateWorldTransform(titleWorldTransform_);
	titleCamera_.UpdateMatrix();

	if (Input::GetInstance()->TriggerKey(DIK_W) || Input::GetInstance()->TriggerKey(DIK_S)) {
		selectedItem_ = selectedItem_ == MenuItem::kStart ? MenuItem::kExit : MenuItem::kStart;
		selectionAnimationTime_ = 0.0f;
		Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
	}

	float pulse = (std::sin(selectionAnimationTime_ * kAnimationSpeed) + 1.0f) * 0.5f;
	float selectedWidth = kImageWidth * (1.0f + pulse * kAnimationScale);
	float selectedHeight = kImageHeight * (1.0f + pulse * kAnimationScale);
	float selectedOffsetX = (selectedWidth - kImageWidth) * 0.5f;
	float selectedOffsetY = (selectedHeight - kImageHeight) * 0.5f;

	if (selectedItem_ == MenuItem::kStart) {
		startSprite_->SetSize({selectedWidth, selectedHeight});
		startSprite_->SetPosition({576.0f - selectedOffsetX, 400.0f - selectedOffsetY});
		exitSprite_->SetSize({kImageWidth, kImageHeight});
		exitSprite_->SetPosition({576.0f, 520.0f});
	} else {
		startSprite_->SetSize({kImageWidth, kImageHeight});
		startSprite_->SetPosition({576.0f, 400.0f});
		exitSprite_->SetSize({selectedWidth, selectedHeight});
		exitSprite_->SetPosition({576.0f - selectedOffsetX, 520.0f - selectedOffsetY});
	}

	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
		if (selectedItem_ == MenuItem::kStart) {
			isFinished_ = true;
		} else {
			PostQuitMessage(0);
		}
	}
}

void TitleScene::Draw() {
	titleBackgroundScene_->DrawTitleBackground();
	DirectXCommon::GetInstance()->ClearDepthBuffer();

	Model::PreDraw();
	titleModel_->Draw(titleWorldTransform_, titleCamera_);
	Model::PostDraw();

	Sprite::PreDraw();
	startSprite_->Draw();
	exitSprite_->Draw();
	Sprite::PostDraw();
}
