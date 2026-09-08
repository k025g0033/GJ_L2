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

	// 1~3のステージを選択
	int selectedStageNumber_ = 1;
	int previousStageNumber_ = 1;
	bool isAnimating_ = false;
	float animationTime_ = 0.0f;
	std::array<uint32_t, 6> stageTextureHandles_{};
	std::array<KamataEngine::Sprite*, 6> stageSprites_{};

	static inline const int kMinStageNumber = 0;
	static inline const int kMaxStageNumber = 5;
	static inline const float kAnimationDuration = 0.35f;
};
