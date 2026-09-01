#include "GameScene.h"
#include "KamataEngine.h"
#include "StageSelectScene.h"
#include "TitleScene.h"
#include <Windows.h>

using namespace KamataEngine;

enum class Scene {
	kTitle,
	kStageSelect,
	kGame,
};

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// エンジンの初期化
	KamataEngine::Initialize(L"GJ1_L2");
	DebugText::GetInstance()->Initialize();

	// DirectXCommonインスタンスの取得
	DirectXCommon* deCommon = DirectXCommon::GetInstance();

	Scene scene = Scene::kTitle;
	TitleScene* titleScene = new TitleScene();
	StageSelectScene* stageSelectScene = nullptr;
	GameScene* gameScene = nullptr;

	// ImGuiManagerインスタンスの取得
	ImGuiManager* imGuiManager = ImGuiManager::GetInstance();

	// メインループ
	while (true) {
		// エンジンの更新
		if (KamataEngine::Update()) {
			break;
		}

		// ImGui受付開始
		imGuiManager->Begin();

		switch (scene) {
		case Scene::kTitle:
			titleScene->Update();
			if (titleScene->IsFinished()) {
				delete titleScene;
				titleScene = nullptr;
				stageSelectScene = new StageSelectScene();
				scene = Scene::kStageSelect;
			}
			break;
		case Scene::kStageSelect:
			stageSelectScene->Update();
			if (stageSelectScene->IsFinished()) {
				delete stageSelectScene;
				stageSelectScene = nullptr;
				gameScene = new GameScene();
				gameScene->Initialize();
				scene = Scene::kGame;
			}
			break;
		case Scene::kGame:
			gameScene->Update();
			break;
		}

		// ImGui受付終了
		imGuiManager->End();

		// 描画開始
		deCommon->PreDraw();

		switch (scene) {
		case Scene::kTitle:
			titleScene->Draw();
			break;
		case Scene::kStageSelect:
			stageSelectScene->Draw();
			break;
		case Scene::kGame:
			gameScene->Draw();
			break;
		}

		// ImGuiの描画
		imGuiManager->Draw();

		// 描画終了
		deCommon->PostDraw();
	}

	delete titleScene;
	delete stageSelectScene;
	delete gameScene;

	// エンジンの終了処理
	KamataEngine::Finalize();
	return 0;
}
