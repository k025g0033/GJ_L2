#pragma once
#include "IScene.h"
#include "KamataEngine.h"
#include <array>

// ステージセレクトシーン
class StageSelectScene : public IScene {
public:
	~StageSelectScene() override;

	void Initialize() override;
	void Update() override;
	void Draw() override;

	bool IsFinished() const override { return isFinished_; }

	int GetSelectedStageNumber() const { return selectedStageNumber_;
	}

private:
	void UpdateStageSpriteLayout();

	bool isFinished_ = false;

	// タイトルとステージ1～10を選択
	int selectedStageNumber_ = 1;
	int previousStageNumber_ = 1;
	bool isAnimating_ = false;
	float animationTime_ = 0.0f;
	static inline const int kMinStageNumber = 0;
	static inline const int kMaxStageNumber = 10;
	static inline const int kMaxPlayableStageNumber = 6;
	static inline const int kStageSpriteCount = kMaxStageNumber + 1;
	std::array<uint32_t, kStageSpriteCount> stageTextureHandles_{};
	std::array<KamataEngine::Sprite*, kStageSpriteCount> stageSprites_{};

	static inline const float kAnimationDuration = 0.35f;
};
