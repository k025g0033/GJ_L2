#pragma once
#include "CameraController.h"
#include "CloneBase.h"
#include "IScene.h"
#include "KamataEngine.h"
#include "MapChipField.h"
#include "Line3D.h"
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
	// クローンの素モデル（球体）
	KamataEngine::Model* modelCloneBase_ = nullptr;
	Line3D* line3D_ = nullptr;
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

	///// ----- クローンの素 ----- /////
	// map.csv上の "C0" で配置された、クローンの素のリスト（複数配置に対応）
	std::vector<CloneBase*> cloneBases_;

	// ImGui上でクローンの素の配置・状態を管理するパネルを表示する
	void ShowCloneBaseManagerImGui();
};
