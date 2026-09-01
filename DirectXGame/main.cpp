#include "KamataEngine.h"
#include "SceneManager.h"
#include <Windows.h>

using namespace KamataEngine;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// エンジンの初期化
	KamataEngine::Initialize(L"GJ1_L2");
	DebugText::GetInstance()->Initialize();

	// DirectXCommonインスタンスの取得
	DirectXCommon* deCommon = DirectXCommon::GetInstance();

	// シーンマネージャの生成、初期化
	SceneManager* sceneManager = new SceneManager();
	sceneManager->Initialize();

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

		// シーンの更新
		sceneManager->Update();

		// ImGui受付終了
		imGuiManager->End();

		// 描画開始
		deCommon->PreDraw();

		// シーンの描画
		sceneManager->Draw();

		// ImGuiの描画
		imGuiManager->Draw();

		// 描画終了
		deCommon->PostDraw();
	}

	delete sceneManager;

	// エンジンの終了処理
	KamataEngine::Finalize();
	return 0;
}
