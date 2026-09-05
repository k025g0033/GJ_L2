#include "SceneManager.h"

#include "GameScene.h"
#include "ResultScene.h"
#include "StageSelectScene.h"
#include "TitleScene.h"

SceneManager::SceneManager() {}

SceneManager::~SceneManager() {
	delete currentScene_;
}

void SceneManager::Initialize() {
	scene_ = Scene::kTitle;
	currentScene_ = CreateScene(scene_);
	currentScene_->Initialize();
}

void SceneManager::Update() {
	currentScene_->Update();

	// 現在のシーンが終了要求を出したら次のシーンへ切り替える
	if (currentScene_->IsFinished()) {
		ChangeScene();
	}
}

void SceneManager::Draw() {
	currentScene_->Draw();
}

void SceneManager::ChangeScene() {
	// ステージ選択結果を削除前に保存
	if (scene_ == Scene::kStageSelect) {
		StageSelectScene* stageSelectScene = dynamic_cast<StageSelectScene*>(currentScene_);

		if (stageSelectScene != nullptr) {
			selectedStageNumber_ = stageSelectScene->GetSelectedStageNumber();
		}
	}

	// 現在のシーンを解放
	delete currentScene_;
	currentScene_ = nullptr;

	// 次のシーン種別を決定（タイトル→ステージセレクト→ゲーム→リザルト→タイトル…の順で循環）
	switch (scene_) {
	case Scene::kTitle:
		scene_ = Scene::kStageSelect;
		break;
	case Scene::kStageSelect:
		scene_ = Scene::kGame;
		break;
	case Scene::kGame:
		scene_ = Scene::kResult;
		break;
	case Scene::kResult:
		scene_ = Scene::kTitle;
		break;
	default:
		scene_ = Scene::kTitle;
		break;
	}

	// 次のシーンを生成して初期化
	currentScene_ = CreateScene(scene_);
	currentScene_->Initialize();
}

IScene* SceneManager::CreateScene(Scene scene) {
	switch (scene) {
	case Scene::kTitle:
		return new TitleScene();
	case Scene::kStageSelect:
		return new StageSelectScene();
	case Scene::kGame:
		return new GameScene(selectedStageNumber_);
	case Scene::kResult:
		return new ResultScene();
	default:
		return nullptr;
	}
}
