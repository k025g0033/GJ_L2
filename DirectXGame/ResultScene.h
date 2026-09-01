#pragma once
#include "IScene.h"

// リザルトシーン
class ResultScene : public IScene {
public:
	// 初期化
	void Initialize() override;

	// 更新
	void Update() override;

	// 描画
	void Draw() override;

	// 終了フラグの取得
	bool IsFinished() const override { return isFinished_; }

private:
	bool isFinished_ = false;
};
