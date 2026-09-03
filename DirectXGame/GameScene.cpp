#include "GameScene.h"
#include "2d/ImGuiManager.h"
#include "WorldTransformConfig.h"
#include <map>

using namespace KamataEngine;

GameScene::GameScene() {}

GameScene::~GameScene() {
	// 解放
	delete modelPlayer_;
	delete modelBlock_;
	delete modelSkydome_;
	for (Lazer* lazer : lazers_) {
		delete lazer;
	}
	lazers_.clear();
	delete modelLazer_;
	delete backgroundSprite_;
	delete mapChipField_;

	delete debugCamera_;

	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			delete worldTransformBlock;
		}
	}
	worldTransformBlocks_.clear();
}

void GameScene::Initialize() {

	// プレイヤーモデル生成
	modelPlayer_ = Model::CreateFromOBJ("player", true);
	// ブロックモデル生成
	modelBlock_ = Model::CreateFromOBJ("block", true);
	// レーザーモデル生成
	modelLazer_ = Model::CreateFromOBJ("Lazer", true);
	// 天球のモデル生成
	modelSkydome_ = Model::CreateFromOBJ("Skydome", true);
	// 背景スプライトの生成
	backgroundTextureHandle_ = TextureManager::Load("uvChecker.png");
	backgroundSprite_ = Sprite::Create(backgroundTextureHandle_, {0.0f, 0.0f});
	backgroundSprite_->SetSize({1280.0f, 720.0f});

	// マップチップフィールドの初期化と生成
	mapChipField_ = new MapChipField;
	mapChipField_->LoadMapChipCsv("Resources/map.csv");

	// カメラの初期化
	camera_.farZ = 1000.0f;
	camera_.Initialize();

	// CSVの配置情報からブロックとプレイヤーを生成
	GenerateBlocks();
	assert(player_ != nullptr && "player is not placed in map.csv");

	// 天球の生成,初期化
	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_, &camera_);
	// カメラの生成,初期化,追従対象をセット,リセット
	cameraController_ = new CameraController();
	cameraController_->Initialize(&camera_);
	cameraController_->SetTarget(player_);
	cameraController_->Reset();
	CameraController::Rect stageRect = {11, 100, 6, 100};
	cameraController_->SetMovableArea(stageRect);

	// デバッグカメラの生成
	debugCamera_ = new DebugCamera(1280, 720);
}

void GameScene::Update() {
#ifdef USE_IMGUI
	ImGui::Begin("Background");
	ImGui::RadioButton("Skydome", &backgroundMode_, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Sprite", &backgroundMode_, 1);
	ImGui::End();
#endif

	// プレイヤーの更新
	player_->Update();
	// レーザーの更新
	for (Lazer* lazer : lazers_) {
		lazer->Update();
	}

	// 天球の更新
	skydome_->Update();

	// カメラの更新
	cameraController_->Update();

	// ブロックの更新
	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {

		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {

			if (!worldTransformBlock)
				continue;

			UpdateWorldTransform(*worldTransformBlock);
		}
	}

#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		// デバッグカメラ有効フラグ
		isDebugCameraActive_ = !isDebugCameraActive_;
	}
#endif

	// ステージクリア判定（仮：ENTERで即終了。本来はゴール地点への到達などで判定する）
	if (Input::GetInstance()->TriggerKey(DIK_RETURN)) {
		isFinished_ = true;
	}

	// カメラの処理
	if (isDebugCameraActive_) {
		// デバッグカメラの更新
		debugCamera_->Update();

		camera_.matView = debugCamera_->GetCamera().matView;
		camera_.matProjection = debugCamera_->GetCamera().matProjection;
		// ビュープロジェクション行列の転送
		camera_.TransferMatrix();
	} else {
		// ビュープロジェクション行列の更新と転送
		camera_.UpdateMatrix();
		// camera_.translation_ = {7.7f, 7.0f, -11.0f};
	}
}

void GameScene::Draw() {
	// スプライト背景は3Dモデルより先に描画する
	if (backgroundMode_ == 1) {
		Sprite::PreDraw();
		backgroundSprite_->Draw();
		Sprite::PostDraw();
	}

	Model::PreDraw();

	// 天球の描画
	if (backgroundMode_ == 0) {
		skydome_->Draw();
	}

	// プレイヤーの描画
	player_->Draw();

	// ブロックの描画
	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			if (!worldTransformBlock)
				continue;

			modelBlock_->Draw(*worldTransformBlock, camera_);
		}
	}

	// レーザーの描画
	for (Lazer* lazer : lazers_) {
		lazer->Draw();
	}

	Model::PostDraw();
}

void GameScene::GenerateBlocks() {

	// サブIDごとにレーザーの座標を格納する
	std::map<uint8_t, std::vector<Vector3>> lazerPositionsByID;

	// 要素数
	uint32_t numBlockVertical = mapChipField_->kNumBlockVertical;
	uint32_t numBlockHorizontal = mapChipField_->kNumBlockHorizontal;

	// 要素数を変更する
	worldTransformBlocks_.resize(numBlockVertical);
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		worldTransformBlocks_[i].resize(numBlockHorizontal);
	}

	// ブロックの生成
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		for (uint32_t j = 0; j < numBlockHorizontal; ++j) {
			MapChipType type = mapChipField_->GetMapChipTypeByIndex(j, i);

			switch (type) {
			case MapChipType::kBlock:
				worldTransformBlocks_[i][j] = new WorldTransform();
				worldTransformBlocks_[i][j]->Initialize();
				worldTransformBlocks_[i][j]->translation_ = mapChipField_->GetMapChipPositionByIndex(j, i);
				UpdateWorldTransform(*worldTransformBlocks_[i][j]);
				break;
			case MapChipType::kPlayer: {
				assert(player_ == nullptr && "player is already placed");
				// 自キャラ生成
				// 座標をマップチップ番号で指定
				Vector3 playerPosition = mapChipField_->GetMapChipPositionByIndex(j, i);
				player_ = new Player();
				player_->Initialize(modelPlayer_,  &camera_, playerPosition);
				player_->SetMapChipField(mapChipField_);
				break;
			}
			case MapChipType::kLazer: {
				uint8_t subID = mapChipField_->GetMapChipSubIDByIndex(j, i);
				lazerPositionsByID[subID].push_back(mapChipField_->GetMapChipPositionByIndex(j, i));
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
			
			case MapChipType::kBlank:
			default:
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
		}
	}

	// L0、L1...のグループごとに1本ずつレーザーを生成する
	for (const auto& [subID, positions] : lazerPositionsByID) {
		if (positions.size() < 2) {
			continue;
		}

		Lazer* lazer = new Lazer();
		lazer->Initialize(modelLazer_, &camera_, positions.front(), positions.back());
		lazers_.push_back(lazer);
	}
}
