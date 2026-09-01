#pragma once

// タイトルシーン
class TitleScene {
public:
	void Update();
	void Draw();

	bool IsFinished() const { return isFinished_; }

private:
	bool isFinished_ = false;
};
