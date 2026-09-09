#include "StageSelectScene.h"
#include "AudioSettings.h"
#include "KamataEngine.h"
#include <algorithm>

using namespace KamataEngine;

namespace {

struct StageLayout {
	Vector2 position;
	float size;
};

int GetStageSlot(int stageNumber, int selectedStageNumber, int stageCount) {
	int slot = stageNumber - selectedStageNumber;
	const int halfStageCount = stageCount / 2;
	if (slot > halfStageCount) {
		slot -= stageCount;
	} else if (slot < -halfStageCount) {
		slot += stageCount;
	}
	return slot;
}

StageLayout GetStageLayout(int slot) {
	if (slot <= -2) {
		return {{-220.0f, 300.0f}, 120.0f};
	}
	if (slot == -1) {
		return {{100.0f, 270.0f}, 180.0f};
	}
	if (slot == 1) {
		return {{1000.0f, 270.0f}, 180.0f};
	}
	if (slot >= 2) {
		return {{1300.0f, 300.0f}, 120.0f};
	}
	return {{480.0f, 180.0f}, 320.0f};
}

} // namespace

StageSelectScene::~StageSelectScene() {
	for (Sprite* sprite : stageSprites_) {
		delete sprite;
	}
}

void StageSelectScene::Initialize() {
	isFinished_ = false;
	selectedStageNumber_ = 1;
	previousStageNumber_ = selectedStageNumber_;
	isAnimating_ = false;
	animationTime_ = 0.0f;
	cursorMoveSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/CursorMove.wav");
	decideSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/Decide.wav");

	stageTextureHandles_[0] = TextureManager::Load("StageSelect/Title.png");
	stageSprites_[0] = Sprite::Create(stageTextureHandles_[0], {0.0f, 0.0f});
	for (int stageNumber = 1; stageNumber <= kMaxStageNumber; ++stageNumber) {
		std::string texturePath = "StageSelect/Stage" + std::to_string(stageNumber) + ".png";
		stageTextureHandles_[stageNumber] = TextureManager::Load(texturePath);
		stageSprites_[stageNumber] = Sprite::Create(stageTextureHandles_[stageNumber], {0.0f, 0.0f});
	}
	UpdateStageSpriteLayout();
}

void StageSelectScene::Update() {
	if (isAnimating_) {
		animationTime_ += 1.0f / 60.0f;
		if (animationTime_ >= kAnimationDuration) {
			animationTime_ = kAnimationDuration;
			isAnimating_ = false;
		}
	}

	if (!isAnimating_) {
		int nextStageNumber = selectedStageNumber_;
		if (Input::GetInstance()->TriggerKey(DIK_A)) {
			nextStageNumber--;
			if (nextStageNumber < kMinStageNumber) {
				nextStageNumber = kMaxStageNumber;
			}
		} else if (Input::GetInstance()->TriggerKey(DIK_D)) {
			nextStageNumber++;
			if (nextStageNumber > kMaxStageNumber) {
				nextStageNumber = kMinStageNumber;
			}
		}

		if (nextStageNumber != selectedStageNumber_) {
			previousStageNumber_ = selectedStageNumber_;
			selectedStageNumber_ = nextStageNumber;
			animationTime_ = 0.0f;
			isAnimating_ = true;
			Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
		}
	}

	UpdateStageSpriteLayout();

	// タイトルと実装済みのステージ1～6だけ決定できる
	if (!isAnimating_ && selectedStageNumber_ <= kMaxPlayableStageNumber && Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
		isFinished_ = true;
	}
}

void StageSelectScene::UpdateStageSpriteLayout() {
	float t = isAnimating_ ? std::clamp(animationTime_ / kAnimationDuration, 0.0f, 1.0f) : 1.0f;
	t = t * t * (3.0f - 2.0f * t);

	for (int stageNumber = 0; stageNumber <= kMaxStageNumber; ++stageNumber) {
		StageLayout start = GetStageLayout(GetStageSlot(stageNumber, previousStageNumber_, kStageSpriteCount));
		StageLayout end = GetStageLayout(GetStageSlot(stageNumber, selectedStageNumber_, kStageSpriteCount));
		Vector2 position = {
		    start.position.x + (end.position.x - start.position.x) * t,
		    start.position.y + (end.position.y - start.position.y) * t};
		float size = start.size + (end.size - start.size) * t;
		stageSprites_[stageNumber]->SetPosition(position);
		stageSprites_[stageNumber]->SetSize({size, size});
	}
}

void StageSelectScene::Draw() {


	Sprite::PreDraw();
	for (Sprite* sprite : stageSprites_) {
		sprite->Draw();
	}
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();
}
 
