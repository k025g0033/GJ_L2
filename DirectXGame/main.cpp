#include "GameScene.h"
#include "KamataEngine.h"
#include <Windows.h>

using namespace KamataEngine;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// エンジンの初期化
	KamataEngine::Initialize(L"GJ1_L2");

	// DirectXCommonインスタンスの取得
	DirectXCommon* deCommon = DirectXCommon::GetInstance();

	// ゲームシーンのインスタンス生成
	GameScene* gameScene = new GameScene();
	// ゲームシーンの初期化
	gameScene->Initialize();

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

		// ゲームシーンの更新
		gameScene->Update();

		// ImGui受付終了
		imGuiManager->End();

		// 描画開始
		deCommon->PreDraw();

		// ゲームシーンの描画
		gameScene->Draw();

		// ImGuiの描画
		imGuiManager->Draw();

		// 描画終了
		deCommon->PostDraw();
	}

	// ゲームシーンの解放
	delete gameScene;
	// nullptrの代入
	gameScene = nullptr;

	// エンジンの終了処理
	KamataEngine::Finalize();
	return 0;
}
