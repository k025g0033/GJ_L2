#include "ResultScene.h"
#include "AudioSettings.h"
#include "KamataEngine.h"
#include <cmath>

using namespace KamataEngine;

ResultScene::~ResultScene() {
	delete titleSprite_;
	delete stageSelectSprite_;
}

void ResultScene::Initialize() {
	isFinished_ = false;
	stageSelectRequested_ = false;
	selectedItem_ = 0;
	selectionAnimationTime_ = 0.0f;
	titleTextureHandle_ = TextureManager::Load("Result/Title.png");
	stageSelectTextureHandle_ = TextureManager::Load("Result/StageSelect.png");
	stageSelectSprite_ = Sprite::Create(stageSelectTextureHandle_, {512.0f, 320.0f});
	titleSprite_ = Sprite::Create(titleTextureHandle_, {512.0f, 420.0f});
	titleSprite_->SetSize({256.0f, 64.0f});
	stageSelectSprite_->SetSize({256.0f, 64.0f});
	cursorMoveSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/CursorMove.wav");
	decideSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/Decide.wav");
}

void ResultScene::Update() {
	selectionAnimationTime_ += 1.0f / 60.0f;
	if (Input::GetInstance()->TriggerKey(DIK_W) || Input::GetInstance()->TriggerKey(DIK_S)) {
		selectedItem_ = selectedItem_ == 0 ? 1 : 0;
		selectionAnimationTime_ = 0.0f;
		Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
	}

	constexpr float kWidth = 256.0f;
	constexpr float kHeight = 64.0f;
	const float pulse = (std::sin(selectionAnimationTime_ * 6.0f) + 1.0f) * 0.5f;
	Sprite* sprites[] = {stageSelectSprite_, titleSprite_};
	for (int i = 0; i < 2; ++i) {
		float width = kWidth;
		float height = kHeight;
		if (i == selectedItem_) {
			width *= 1.0f + pulse * 0.08f;
			height *= 1.0f + pulse * 0.08f;
		}
		sprites[i]->SetSize({width, height});
		sprites[i]->SetPosition({512.0f - (width - kWidth) * 0.5f, 320.0f + 100.0f * i - (height - kHeight) * 0.5f});
	}

	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
		if (selectedItem_ == 0) {
			stageSelectRequested_ = true;
		} else {
			isFinished_ = true;
		}
	}
}

void ResultScene::Draw() {
	Sprite::PreDraw();
	titleSprite_->Draw();
	stageSelectSprite_->Draw();
	Sprite::PostDraw();
}
