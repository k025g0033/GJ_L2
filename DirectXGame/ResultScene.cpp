#include "ResultScene.h"
#include "AudioSettings.h"
#include "GameScene.h"
#include "KamataEngine.h"
#include <cmath>

using namespace KamataEngine;

ResultScene::~ResultScene() {
	delete resultBackgroundScene_;
	delete sunsetOverlaySprite_;
	delete menuPanelSprite_;
	delete nextStageSprite_;
	delete titleSprite_;
	delete stageSelectSprite_;
}

void ResultScene::Initialize() {
	isFinished_ = false;
	stageSelectRequested_ = false;
	nextStageRequested_ = false;
	titleRequested_ = false;

	selectedItem_ = 0;
	selectionAnimationTime_ = 0.0f;
	// タイトル用マップではなく、今クリアしたステージを静止背景として使う。
	resultBackgroundScene_ = new GameScene(clearedStageNumber_);
	resultBackgroundScene_->Initialize();
	menuPanelTextureHandle_ = TextureManager::Load("white1x1.png");
	sunsetOverlaySprite_ = Sprite::Create(menuPanelTextureHandle_, {0.0f, 0.0f});
	sunsetOverlaySprite_->SetSize({1280.0f, 720.0f});
	sunsetOverlaySprite_->SetColor({1.0f, 0.38f, 0.08f, 0.22f});
	menuPanelSprite_ = Sprite::Create(menuPanelTextureHandle_, {448.0f, 180.0f});
	menuPanelSprite_->SetSize({384.0f, 400.0f});
	menuPanelSprite_->SetColor({0.0f, 0.0f, 0.0f, 0.45f});

	nextStageTextureHandle_ = TextureManager::Load("Result/nextstage.png");
	titleTextureHandle_ = TextureManager::Load("Result/Title.png");
	stageSelectTextureHandle_ = TextureManager::Load("Result/StageSelect.png");
	
	nextStageSprite_ = Sprite::Create(nextStageTextureHandle_, {512.0f, 220.0f});
	stageSelectSprite_ = Sprite::Create(stageSelectTextureHandle_, {512.0f, 320.0f});
	titleSprite_ = Sprite::Create(titleTextureHandle_, {512.0f, 420.0f});
	
	nextStageSprite_->SetSize({256.0f, 64.0f});
	titleSprite_->SetSize({256.0f, 64.0f});
	stageSelectSprite_->SetSize({256.0f, 64.0f});

	cursorMoveSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/CursorMove.wav");
	decideSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/Decide.wav");
}

void ResultScene::Update() {
	selectionAnimationTime_ += 1.0f / 60.0f;
	resultBackgroundScene_->UpdateTitleBackground();
	if (Input::GetInstance()->TriggerKey(DIK_W)) {
		selectedItem_ = (selectedItem_ + 2) % 3;
		selectionAnimationTime_ = 0.0f;
		Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
	}

	if (Input::GetInstance()->TriggerKey(DIK_S)) {
		selectedItem_ = (selectedItem_ + 1) % 3;
		selectionAnimationTime_ = 0.0f;
		Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
	}

	constexpr float kWidth = 256.0f;
	constexpr float kHeight = 64.0f;
	const float pulse = (std::sin(selectionAnimationTime_ * 6.0f) + 1.0f) * 0.5f;
	Sprite* sprites[] = {nextStageSprite_, stageSelectSprite_, titleSprite_};

	for (int i = 0; i < 3; ++i) {
		float width = kWidth;
		float height = kHeight;

		if (i == selectedItem_) {
			width *= 1.0f + pulse * 0.08f;
			height *= 1.0f + pulse * 0.08f;
		}

		sprites[i]->SetSize({width, height});
		sprites[i]->SetPosition({512.0f - (width - kWidth) * 0.5f, 280.0f + 100.0f * i - (height - kHeight) * 0.5f});
	}

	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());

		switch (selectedItem_) {
		case 0:
			nextStageRequested_ = true;
			break;

		case 1:
			stageSelectRequested_ = true;
			break;

		case 2:
			titleRequested_ = true;
			break;
		}
	}
}

void ResultScene::Draw() {
	resultBackgroundScene_->DrawResultBackground();
	DirectXCommon::GetInstance()->ClearDepthBuffer();
	Sprite::PreDraw();

	sunsetOverlaySprite_->Draw();
	menuPanelSprite_->Draw();
	nextStageSprite_->Draw();
	stageSelectSprite_->Draw();
	titleSprite_->Draw();

	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();
}
