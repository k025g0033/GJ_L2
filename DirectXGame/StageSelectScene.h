#pragma once

// ステージセレクトシーン
class StageSelectScene {
public:
	void Update();
	void Draw();

	bool IsFinished() const { return isFinished_; }

private:
	bool isFinished_ = false;
};
