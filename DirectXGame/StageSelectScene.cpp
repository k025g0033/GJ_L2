#include "StageSelectScene.h"
#include "AudioSettings.h"
#include "KamataEngine.h"
#include <algorithm>
#include <numbers>

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

StageSelectScene::StageSelectScene(int initialStageNumber, int highestUnlockedStage)
    : initialStageNumber_(initialStageNumber), initialHighestUnlockedStage_(highestUnlockedStage) {}

StageSelectScene::~StageSelectScene() {
	for (Sprite* sprite : stageSprites_) {
		delete sprite;
	}
	for (auto& frame : clearFrameSprites_) {
		for (Sprite* sprite : frame) {
			delete sprite;
		}
	}
	delete keyASprite_;
	delete keyDSprite_;
	delete leftArrowSprite_;
	delete rightArrowSprite_;
}

void StageSelectScene::Initialize() {
	isFinished_ = false;
	highestUnlockedStage_ = std::clamp(initialHighestUnlockedStage_, 1, kMaxStageNumber);
	selectedStageNumber_ = std::clamp(initialStageNumber_, kMinStageNumber, highestUnlockedStage_);
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
	clearFrameTextureHandle_ = TextureManager::Load("white1x1.png");
	for (int stageNumber = 1; stageNumber <= kMaxStageNumber; ++stageNumber) {
		for (Sprite*& frameSprite : clearFrameSprites_[stageNumber]) {
			frameSprite = Sprite::Create(clearFrameTextureHandle_, {0.0f, 0.0f});
			frameSprite->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
		}
	}

	keyATextureHandle_ = TextureManager::Load("StageSelect/KeyA.png");
	keyDTextureHandle_ = TextureManager::Load("StageSelect/KeyD.png");
	arrowTextureHandle_ = TextureManager::Load("StageSelect/Arrow.png");
	keyASprite_ = Sprite::Create(keyATextureHandle_, {48.0f, 624.0f});
	leftArrowSprite_ = Sprite::Create(
	    arrowTextureHandle_, {160.0f, 656.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f});
	rightArrowSprite_ = Sprite::Create(
	    arrowTextureHandle_, {1120.0f, 656.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f});
	keyDSprite_ = Sprite::Create(keyDTextureHandle_, {1168.0f, 624.0f});
	keyASprite_->SetSize({64.0f, 64.0f});
	keyDSprite_->SetSize({64.0f, 64.0f});
	leftArrowSprite_->SetSize({64.0f, 64.0f});
	rightArrowSprite_->SetSize({64.0f, 64.0f});
	leftArrowSprite_->SetRotation(-std::numbers::pi_v<float> / 2.0f);
	rightArrowSprite_->SetRotation(std::numbers::pi_v<float> / 2.0f);
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
				nextStageNumber = highestUnlockedStage_;
			}
		} else if (Input::GetInstance()->TriggerKey(DIK_D)) {
			nextStageNumber++;
			if (nextStageNumber > highestUnlockedStage_) {
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

	// デバッグ用：選択中の最新ステージをクリア扱いにして次を解放する
	if (!isAnimating_ && Input::GetInstance()->TriggerKey(DIK_1) &&
	    selectedStageNumber_ == highestUnlockedStage_) {
		highestClearedStage_ = selectedStageNumber_;
		if (highestUnlockedStage_ < kMaxStageNumber) {
			highestUnlockedStage_++;
		}
		previousStageNumber_ = selectedStageNumber_;
		animationTime_ = kAnimationDuration;
		Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
	}

	UpdateStageSpriteLayout();

	// タイトルと解放済みのステージだけ決定できる
	if (!isAnimating_ && selectedStageNumber_ <= kMaxPlayableStageNumber && Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
		isFinished_ = true;
	}
}

void StageSelectScene::UpdateStageSpriteLayout() {
	float t = isAnimating_ ? std::clamp(animationTime_ / kAnimationDuration, 0.0f, 1.0f) : 1.0f;
	t = t * t * (3.0f - 2.0f * t);

	const int visibleStageCount = highestUnlockedStage_ + 1; // タイトルを含む
	for (int stageNumber = 0; stageNumber <= highestUnlockedStage_; ++stageNumber) {
		StageLayout start = GetStageLayout(GetStageSlot(stageNumber, previousStageNumber_, visibleStageCount));
		StageLayout end = GetStageLayout(GetStageSlot(stageNumber, selectedStageNumber_, visibleStageCount));
		Vector2 position = {
		    start.position.x + (end.position.x - start.position.x) * t,
		    start.position.y + (end.position.y - start.position.y) * t};
		float size = start.size + (end.size - start.size) * t;
		stageSprites_[stageNumber]->SetPosition(position);
		stageSprites_[stageNumber]->SetSize({size, size});
		if (stageNumber >= 1 && stageNumber <= highestClearedStage_) {
			const float thickness = size * 0.04f;
			auto& frame = clearFrameSprites_[stageNumber];
			frame[0]->SetPosition(position);
			frame[0]->SetSize({size, thickness});
			frame[1]->SetPosition({position.x, position.y + size - thickness});
			frame[1]->SetSize({size, thickness});
			frame[2]->SetPosition(position);
			frame[2]->SetSize({thickness, size});
			frame[3]->SetPosition({position.x + size - thickness, position.y});
			frame[3]->SetSize({thickness, size});
		}
	}
}

void StageSelectScene::Draw() {


	Sprite::PreDraw();
	for (int stageNumber = 0; stageNumber <= highestUnlockedStage_; ++stageNumber) {
		stageSprites_[stageNumber]->Draw();
		if (stageNumber >= 1 && stageNumber <= highestClearedStage_) {
			for (Sprite* frameSprite : clearFrameSprites_[stageNumber]) {
				frameSprite->Draw();
			}
		}
	}
	keyASprite_->Draw();
	leftArrowSprite_->Draw();
	rightArrowSprite_->Draw();
	keyDSprite_->Draw();
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();
}
 
