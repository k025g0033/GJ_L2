#include "StageSelectScene.h"
#include "KamataEngine.h"

using namespace KamataEngine;

void StageSelectScene::Update() {
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		isFinished_ = true;
	}
}

void StageSelectScene::Draw() {
	DebugText::GetInstance()->Print("STAGE SELECT SCENE", 430.0f, 300.0f, 2.0f);
	DebugText::GetInstance()->Print("PRESS SPACE", 500.0f, 350.0f, 1.5f);
	Sprite::PreDraw();
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();
}
