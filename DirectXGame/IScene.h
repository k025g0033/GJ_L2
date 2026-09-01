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

private:
};
