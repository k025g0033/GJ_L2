#pragma once
#include "CameraController.h"
#include "CloneBase.h"
#include "Door.h"
#include "IScene.h"
#include "KamataEngine.h"
#include "Lazer.h"
#include "Line3D.h"
#include "MapChipField.h"
#include "Player.h"
#include "PushPlate.h"
#include "Skydome.h"
#include <vector>

// ゲームシーン
class GameScene : public IScene {
public:
	explicit GameScene(int stageNumber = 1);
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

	// 全ての当たり判定を行う
	void CheckAllCollisions();

	// クローンの素を持っているときの追従先座標を計算する
	KamataEngine::Vector3 ComputeHeldCloneBasePosition() const;

	// 前フレームの自機の向き（方向転換を検出するために保持）
	Player::LRDirection previousPlayerDirection_ = Player::LRDirection::kRight;

private:
	std::vector<PushPlate*> pressurePlates_;
	std::vector<Door*> doors_;

	void UpdatePressurePlates();
	void UpdateDoors();

	// 終了フラグ（仮：本来はゴール到達などのクリア条件で立てる）
	bool isFinished_ = false;

	// 自キャラ
	Player* player_ = nullptr;
	// 天球
	Skydome* skydome_ = nullptr;
	// レーザー
	std::vector<Lazer*> lazers_;
	// マップチップフィールド
	MapChipField* mapChipField_;
	// カメラ
	CameraController* cameraController_;

	// プレイヤーモデル
	KamataEngine::Model* modelPlayer_ = nullptr;
	// ブロックモデル
	KamataEngine::Model* modelBlock_ = nullptr;
	// レーザーモデル
	KamataEngine::Model* modelLazer_ = nullptr;
	// 天球モデル
	KamataEngine::Model* modelSkydome_ = nullptr;
	// 水モデル
	KamataEngine::Model* modelWater_ = nullptr;
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
	// 水用ワールドトランスフォーム
	std::vector<std::vector<KamataEngine::WorldTransform*>> worldTransformWaters_;

	// カメラ
	KamataEngine::Camera camera_;
	// デバッグカメラ
	KamataEngine::DebugCamera* debugCamera_ = nullptr;
	// デバッグカメラ有効
	bool isDebugCameraActive_ = false;

	///// ----- クローンの素 ----- /////
	// map.csv上の "C0" で配置された、クローンの素のリスト（複数配置に対応）
	std::vector<CloneBase*> cloneBases_;
	CloneBase* controlledClone_ = nullptr;

	// ImGui上でクローンの素の配置・状態を管理するパネルを表示する
	void ShowCloneBaseManagerImGui();

	///// ----- クローンの素を持つ処理（仮実装） ----- /////
	// プレイヤーといずれかのクローンの素が当たっているか
	bool isCollidingWithCloneBase_ = false;
	// 拾える状態か（現状は「当たっていたら拾える」。将来的には自機を中心とした円の半径内で判定する）
	bool canPickUpCloneBase_ = false;
	// クローンの素を持っているか
	bool isHoldingCloneBase_ = false;
	// 持っているクローンの素（未所持ならnullptr）
	CloneBase* heldCloneBase_ = nullptr;
	// 今当たっているクローンの素（当たっていなければnullptr）
	CloneBase* collidingCloneBase_ = nullptr;

	// プレイヤーとクローンの素の当たり判定、スペースキーで持つ処理の更新
	void UpdateCloneBasePickup();

	int stageNumber_ = 1;
};
