#pragma once
#include "IScene.h"
#include "KamataEngine.h"
#include <array>

// ステージセレクトシーン
class StageSelectScene : public IScene {
public:
	explicit StageSelectScene(int initialStageNumber = 1, int highestUnlockedStage = 1, int highestClearedStage = 0);
	~StageSelectScene() override;

	void Initialize() override;
	void Update() override;
	void Draw() override;

	bool IsFinished() const override { return isFinished_; }

	int GetSelectedStageNumber() const { return selectedStageNumber_; }
	int GetHighestUnlockedStage() const { return highestUnlockedStage_; }
	int GetHighestClearedStage() const { return highestClearedStage_; }

private:
	void UpdateStageSpriteLayout();

	bool isFinished_ = false;

	// タイトルとステージ1～20を選択
	int selectedStageNumber_ = 1;
	int previousStageNumber_ = 1;
	int highestUnlockedStage_ = 1;
	int highestClearedStage_ = 0;
	bool isAnimating_ = false;
	float animationTime_ = 0.0f;
	static inline const int kMinStageNumber = 0;
	static inline const int kMaxStageNumber = 12;
	static inline const int kMaxPlayableStageNumber = 12;
	static inline const int kStageSpriteCount = kMaxStageNumber + 1;
	std::array<uint32_t, kStageSpriteCount> stageTextureHandles_{};
	std::array<KamataEngine::Sprite*, kStageSpriteCount> stageSprites_{};
	uint32_t clearFrameTextureHandle_ = 0;
	std::array<std::array<KamataEngine::Sprite*, 4>, kStageSpriteCount> clearFrameSprites_{};
	uint32_t keyATextureHandle_ = 0;
	uint32_t keyDTextureHandle_ = 0;
	uint32_t arrowTextureHandle_ = 0;
	KamataEngine::Sprite* keyASprite_ = nullptr;
	KamataEngine::Sprite* keyDSprite_ = nullptr;
	KamataEngine::Sprite* leftArrowSprite_ = nullptr;
	KamataEngine::Sprite* rightArrowSprite_ = nullptr;
	uint32_t cursorMoveSoundHandle_ = 0;
	uint32_t decideSoundHandle_ = 0;

	static inline const float kAnimationDuration = 0.35f;

	// 初期ステージ番号
	int initialStageNumber_ = 1;
	int initialHighestUnlockedStage_ = 1;
	int initialHighestClearedStage_ = 0;

};
