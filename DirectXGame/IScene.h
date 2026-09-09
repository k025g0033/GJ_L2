#pragma once

/// <summary>
/// シーン共通の基底クラス
/// </summary>
class IScene { // インターフェースシーン シーン用の共通ルール
public:
	// 仮想デストラクタ
	virtual ~IScene() = default;

	// 初期化
	virtual void Initialize() = 0;

	// 更新
	virtual void Update() = 0;

	// 描画
	virtual void Draw() = 0;

	// 終了フラグの取得
	virtual bool IsFinished() const = 0;

	// リロード要求グラフ
	virtual bool GetReloadRequested() const { return false; }

	// ステージセレクトへ戻る要求
	virtual bool GetStageSelectRequested() const { return false; }
	
	// 次のステージへ進む要求
	virtual bool GetNextStageRequested() const { return false; }

	virtual bool GetTitleRequested() const { return false; }

private:
};
