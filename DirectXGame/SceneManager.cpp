#include "SceneManager.h"

#include "AudioSettings.h"
#include "GameScene.h"
#include "ResultScene.h"
#include "StageSelectScene.h"
#include "TitleScene.h"

SceneManager::SceneManager() {}

SceneManager::~SceneManager() {
	if (hasBgmVoice_) {
		KamataEngine::Audio::GetInstance()->StopWave(bgmVoiceHandle_);
	}
	delete currentScene_;
}

void SceneManager::Initialize() {
	titleBgmSoundHandle_ = KamataEngine::Audio::GetInstance()->LoadWave("Sound/TitleBgm.wav");
	gameBgmSoundHandle_ = KamataEngine::Audio::GetInstance()->LoadWave("Sound/GameBgm.wav");
	resultSoundHandle_ = KamataEngine::Audio::GetInstance()->LoadWave("Sound/Result.wav");

	scene_ = Scene::kTitle;
	currentScene_ = CreateScene(scene_);
	currentScene_->Initialize();
	ChangeBgm(scene_);
}

void SceneManager::Update() {
	currentScene_->Update();
	if (hasBgmVoice_ && KamataEngine::Audio::GetInstance()->IsPlaying(bgmVoiceHandle_)) {
		float baseVolume = 0.0f;
		switch (currentBgm_) {
		case BgmType::kTitle:
			baseVolume = 0.1f;
			break;
		case BgmType::kGame:
		case BgmType::kResult:
			baseVolume = 0.3f;
			break;
		case BgmType::kNone:
			break;
		}
		KamataEngine::Audio::GetInstance()->SetVolume(
		    bgmVoiceHandle_, baseVolume * AudioSettings::GetBgmScale());
	}

	// 現在のステージを最初から読み直す
	if (currentScene_->GetReloadRequested()) {
		delete currentScene_;
		currentScene_ = CreateScene(scene_);
		currentScene_->Initialize();
		return;
	}

	// ゲームシーンからステージセレクトへ戻る
	if (currentScene_->GetStageSelectRequested()) {
		delete currentScene_;
		scene_ = Scene::kStageSelect;
		currentScene_ = CreateScene(scene_);
		currentScene_->Initialize();
		ChangeBgm(scene_);
		return;
	}

	if (currentScene_->GetTitleRequested()) {
		delete currentScene_;

		scene_ = Scene::kTitle;
		currentScene_ = CreateScene(scene_);
		currentScene_->Initialize();

		ChangeBgm(scene_);
		return;
	}

	// リザルトから次のステージへ進む
	if (currentScene_->GetNextStageRequested()) {
		constexpr int kMaxStageNumber = 20;

		if (selectedStageNumber_ < kMaxStageNumber) {
			++selectedStageNumber_;
			scene_ = Scene::kGame;
		} else {
			// 最終ステージをクリアした場合
			scene_ = Scene::kStageSelect;
		}

		delete currentScene_;
		currentScene_ = CreateScene(scene_);
		currentScene_->Initialize();
		ChangeBgm(scene_);
		return;
	}

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
			highestUnlockedStage_ = stageSelectScene->GetHighestUnlockedStage();
			highestClearedStage_ = stageSelectScene->GetHighestClearedStage();
		}
	}

	// ゲームをクリアしてリザルトへ進む時だけ、次のステージを解放する。
	if (scene_ == Scene::kGame) {
		if (selectedStageNumber_ > highestClearedStage_) {
			highestClearedStage_ = selectedStageNumber_;
		}
		const int nextStageNumber = selectedStageNumber_ + 1;
		if (nextStageNumber > highestUnlockedStage_) {
			highestUnlockedStage_ = nextStageNumber > 20 ? 20 : nextStageNumber;
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
		scene_ = selectedStageNumber_ == 0 ? Scene::kTitle : Scene::kGame;
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
	ChangeBgm(scene_);
}

void SceneManager::ChangeBgm(Scene nextScene) {
	BgmType nextBgm = BgmType::kNone;
	switch (nextScene) {
	case Scene::kTitle:
	case Scene::kStageSelect:
		nextBgm = BgmType::kTitle;
		break;
	case Scene::kGame:
		nextBgm = BgmType::kGame;
		break;
	case Scene::kResult:
		nextBgm = BgmType::kResult;
		break;
	default:
		break;
	}

	// タイトルとステージセレクト間では同じ曲を継続する。
	if (currentBgm_ == nextBgm && nextBgm != BgmType::kResult) {
		return;
	}

	if (hasBgmVoice_) {
		KamataEngine::Audio::GetInstance()->StopWave(bgmVoiceHandle_);
		hasBgmVoice_ = false;
	}

	currentBgm_ = nextBgm;
	switch (currentBgm_) {
	case BgmType::kTitle:
		bgmVoiceHandle_ = KamataEngine::Audio::GetInstance()->PlayWave(
		    titleBgmSoundHandle_, true, 0.1f * AudioSettings::GetBgmScale());
		hasBgmVoice_ = true;
		break;
	case BgmType::kGame:
		bgmVoiceHandle_ = KamataEngine::Audio::GetInstance()->PlayWave(
		    gameBgmSoundHandle_, true, 0.3f * AudioSettings::GetBgmScale());
		hasBgmVoice_ = true;
		break;
	case BgmType::kResult:
		bgmVoiceHandle_ = KamataEngine::Audio::GetInstance()->PlayWave(
		    resultSoundHandle_, false, 0.3f * AudioSettings::GetBgmScale());
		hasBgmVoice_ = true;
		break;
	case BgmType::kNone:
		break;
	}
}

IScene* SceneManager::CreateScene(Scene scene) {
	switch (scene) {
	case Scene::kTitle:
		return new TitleScene();
	case Scene::kStageSelect:
		return new StageSelectScene(selectedStageNumber_, highestUnlockedStage_, highestClearedStage_);
	case Scene::kGame:
		return new GameScene(selectedStageNumber_);
	case Scene::kResult:
		return new ResultScene(selectedStageNumber_);
	default:
		return nullptr;
	}
}
