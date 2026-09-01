#pragma once
#include "CameraController.h"
#include "IScene.h"
#include "KamataEngine.h"
#include "MapChipField.h"
#include "Player.h"
#include "Skydome.h"
#include <vector>

// ゲームシーン
class GameScene : public IScene {
public:
	GameScene();
	~GameScene() override;

	// 初期化
	void Initialize() override;

	// 更新
	void Update() override;

	// 描画
	void Draw() override;

	// 終了フラグの取得
	bool IsFinished() const override { return isFinished_; }

	void GenerateBlocks();

	// クローンの生成
	void SpawnClone();

private:
	// 終了フラグ（仮：本来はゴール到達などのクリア条件で立てる）
	bool isFinished_ = false;

	// 自キャラ
	Player* player_ = nullptr;
	// 天球
	Skydome* skydome_ = nullptr;
	// マップチップフィールド
	MapChipField* mapChipField_;
	// カメラ
	CameraController* cameraController_;

	// プレイヤーモデル
	KamataEngine::Model* modelPlayer_ = nullptr;
	// ブロックモデル
	KamataEngine::Model* modelBlock_ = nullptr;
	// 天球モデル
	KamataEngine::Model* modelSkydome_ = nullptr;
	// 背景スプライト
	KamataEngine::Sprite* backgroundSprite_ = nullptr;
	uint32_t backgroundTextureHandle_ = 0;
	// 0: 天球、1: スプライト
	int backgroundMode_ = 0;

	// ブロック用ワールドトランスフォーム
	std::vector<std::vector<KamataEngine::WorldTransform*>> worldTransformBlocks_;

	// カメラ
	KamataEngine::Camera camera_;
	// デバッグカメラ
	KamataEngine::DebugCamera* debugCamera_ = nullptr;
	// デバッグカメラ有効
	bool isDebugCameraActive_ = false;

	///// ----- クローン ----- /////
	/// --- 状態 ---
	// クローンを生成済みか
	bool hasClone_ = false;

	/// --- データ ---
	// クローン用ワールドトランスフォーム（モデルは自機のものを流用）
	KamataEngine::WorldTransform cloneWorldTransform_;

	// 自機からどれだけ離してクローンを出すか（仮の固定オフセット）
	static inline const float kCloneOffsetX = 2.0f;
};
