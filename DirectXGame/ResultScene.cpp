#include "ResultScene.h"
#include "KamataEngine.h"

using namespace KamataEngine;

void ResultScene::Initialize() {
	isFinished_ = false;
}

void ResultScene::Update() {
	// 仮：SPACEでタイトルに戻る
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		isFinished_ = true;
	}
}

void ResultScene::Draw() {
	DebugText::GetInstance()->Print("RESULT SCENE", 480.0f, 300.0f, 2.0f);
	DebugText::GetInstance()->Print("PRESS SPACE TO TITLE", 420.0f, 350.0f, 1.5f);
	Sprite::PreDraw();
	DebugText::GetInstance()->DrawAll();
	Sprite::PostDraw();
}
