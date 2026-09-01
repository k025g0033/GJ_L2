#pragma once
#include "IScene.h"

// タイトルシーン
class TitleScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;

	bool IsFinished() const override { return isFinished_; }

private:
	bool isFinished_ = false;
};
