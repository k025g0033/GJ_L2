#pragma once
#include <cstdint>

///// ----- インクルード ----- /////
#include "IScene.h"

/// <summary>
/// シーンを管理する
/// </summary>
class SceneManager {
public:
	///// ----- コンストラクタ、デストラクタ ----- /////
	SceneManager();
	~SceneManager();

	///// ----- 初期化 ----- /////
	void Initialize();

	///// ----- 更新 ----- /////
	void Update();

	///// ----- 描画 ----- /////
	void Draw();

	///// ----- ゲッター ----- /////
	int GetSelectedStageNumber() const { return selectedStageNumber_; }

	///// ----- セッター ----- /////
	void SetSelectedStageNumber(int stageNumber) { selectedStageNumber_ = stageNumber; }

private:
	///// ----- 型 ----- /////
	/// --- 各シーン ---
	enum Scene {
		kUnknown = 0,

		kTitle, // タイトル
		kStageSelect, // ステージセレクト
		kGame, // ゲームシーン
		kResult, // リザルト
	};

	///// ----- 関数 ----- /////
	/// --- シーン切り替え ---
	void ChangeScene();

	/// --- 現在の種別に応じたシーンの生成 ---
	IScene* CreateScene(Scene scene);

	/// --- シーンに応じたBGMへ切り替える ---
	void ChangeBgm(Scene nextScene);

	enum class BgmType {
		kNone,
		kTitle,
		kGame,
		kResult,
	};

	///// ----- 変数 ----- /////
	/// --- 現在シーン ---
	Scene scene_ = Scene::kTitle;

	/// --- シーンのポインタを宣言 ---
	IScene* currentScene_ = nullptr;

	/// --- ステージ進行を管理 ---
	// ステージセレクトで選んだステージ番号（仮ステージが1つだけの間は未使用。複数ステージ対応時にStageSelectSceneから渡す）
	int selectedStageNumber_ = 1;
	// この起動中に選択できる最後のステージ（終了すると1に戻る）
	int highestUnlockedStage_ = 1;

	uint32_t titleBgmSoundHandle_ = 0;
	uint32_t gameBgmSoundHandle_ = 0;
	uint32_t resultSoundHandle_ = 0;
	uint32_t bgmVoiceHandle_ = 0;
	BgmType currentBgm_ = BgmType::kNone;
	bool hasBgmVoice_ = false;
};
