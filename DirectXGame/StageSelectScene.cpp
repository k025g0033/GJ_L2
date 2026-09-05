#include "StageSelectScene.h"
#include "KamataEngine.h"
#include <string>

using namespace KamataEngine;

void StageSelectScene::Initialize() {
	isFinished_ = false;
}

void StageSelectScene::Update() {

	// 左へ移動
	if (Input::GetInstance()->TriggerKey(DIK_A)) {
		selectedStageNumber_--;

		if (selectedStageNumber_ < kMinStageNumber) {
			selectedStageNumber_ = kMaxStageNumber;
		}
	}

	// 右へ移動
	if (Input::GetInstance()->TriggerKey(DIK_D)) {
		selectedStageNumber_++;

		if (selectedStageNumber_ > kMaxStageNumber) {
			selectedStageNumber_ = kMinStageNumber;
		}
	}

	// 決定
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		isFinished_ = true;
	}
}

void StageSelectScene::Draw() {
	DebugText::GetInstance()->Print("STAGE SELECT", 460.0f, 250.0f, 2.0f);

	std::string stageText = "STAGE " + std::to_string(selectedStageNumber_);

	DebugText::GetInstance()->Print(stageText, 520.0f, 330.0f, 2.0f);

	DebugText::GetInstance()->Print("A / D : SELECT", 500.0f, 400.0f, 1.2f);

	DebugText::GetInstance()->Print("SPACE : START", 500.0f, 440.0f, 1.2f);

	Sprite::PreDraw();
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();
}
