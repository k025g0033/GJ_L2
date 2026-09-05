#pragma once
#include "IScene.h"

// ステージセレクトシーン
class StageSelectScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;

	bool IsFinished() const override { return isFinished_; }

	int GetSelectedStageNumber() const { return selectedStageNumber_;
	}

private:
	bool isFinished_ = false;

	// 1~3のステージを選択
	int selectedStageNumber_ = 1;

	static inline const int kMinStageNumber = 1;
	static inline const int kMaxStageNumber = 3;
};
