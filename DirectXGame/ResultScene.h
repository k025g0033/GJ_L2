#pragma once
#include "IScene.h"
#include "KamataEngine.h"

// リザルトシーン
class ResultScene : public IScene {
public:
	~ResultScene() override;

	// 初期化
	void Initialize() override;

	// 更新
	void Update() override;

	// 描画
	void Draw() override;

	// 終了フラグの取得
	bool IsFinished() const override { return isFinished_; }
	bool GetStageSelectRequested() const override { return stageSelectRequested_; }

private:
	bool isFinished_ = false;
	bool stageSelectRequested_ = false;
	int selectedItem_ = 0;
	float selectionAnimationTime_ = 0.0f;
	uint32_t titleTextureHandle_ = 0;
	uint32_t stageSelectTextureHandle_ = 0;
	uint32_t cursorMoveSoundHandle_ = 0;
	uint32_t decideSoundHandle_ = 0;
	KamataEngine::Sprite* titleSprite_ = nullptr;
	KamataEngine::Sprite* stageSelectSprite_ = nullptr;
};
