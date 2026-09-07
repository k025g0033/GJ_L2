#include "StageSelectScene.h"
#include "KamataEngine.h"
#include <algorithm>

using namespace KamataEngine;

namespace {

struct StageLayout {
	Vector2 position;
	float size;
};

int GetStageSlot(int stageNumber, int selectedStageNumber) {
	return stageNumber - selectedStageNumber;
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

	for (int i = 0; i < 5; ++i) {
		std::string texturePath = "StageSelect/Stage" + std::to_string(i + 1) + ".png";
		stageTextureHandles_[i] = TextureManager::Load(texturePath);
		stageSprites_[i] = Sprite::Create(stageTextureHandles_[i], {0.0f, 0.0f});
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
				nextStageNumber = kMinStageNumber;
			}
		} else if (Input::GetInstance()->TriggerKey(DIK_D)) {
			nextStageNumber++;
			if (nextStageNumber > kMaxStageNumber) {
				nextStageNumber = kMaxStageNumber;
			}
		}

		if (nextStageNumber != selectedStageNumber_) {
			previousStageNumber_ = selectedStageNumber_;
			selectedStageNumber_ = nextStageNumber;
			animationTime_ = 0.0f;
			isAnimating_ = true;
		}
	}

	UpdateStageSpriteLayout();

	// 決定
	if (!isAnimating_ && Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		isFinished_ = true;
	}
}

void StageSelectScene::UpdateStageSpriteLayout() {
	float t = isAnimating_ ? std::clamp(animationTime_ / kAnimationDuration, 0.0f, 1.0f) : 1.0f;
	t = t * t * (3.0f - 2.0f * t);

	for (int i = 0; i < 5; ++i) {
		int stageNumber = i + 1;
		StageLayout start = GetStageLayout(GetStageSlot(stageNumber, previousStageNumber_));
		StageLayout end = GetStageLayout(GetStageSlot(stageNumber, selectedStageNumber_));
		Vector2 position = {
		    start.position.x + (end.position.x - start.position.x) * t,
		    start.position.y + (end.position.y - start.position.y) * t};
		float size = start.size + (end.size - start.size) * t;
		stageSprites_[i]->SetPosition(position);
		stageSprites_[i]->SetSize({size, size});
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
