#include "GameScene.h"
#include "2d/ImGuiManager.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;

GameScene::GameScene() {}

GameScene::~GameScene() {
	// 解放
	delete modelPlayer_;
	delete modelBlock_;
	delete modelSkydome_;
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

	///// ----- クローン ----- /////
	/// --- 召喚 ---
	// Fキーのトリガーで、まだ居ないときだけ自機の隣にクローンを生成する
	if (Input::GetInstance()->TriggerKey(DIK_F) && !hasClone_) {
		SpawnClone();
	}

	/// --- 更新 ---
	// クローンのワールド行列を更新（今は固定位置だが、将来動かす場合もここで更新する）
	if (hasClone_) {
		UpdateWorldTransform(cloneWorldTransform_);
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

	// クローンの描画（現在は自機と同じモデルを流用）
	if (hasClone_) {
		modelPlayer_->Draw(cloneWorldTransform_, camera_);
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

	Model::PostDraw();

	///// ----- クローンと自機との接続線 ----- /////
	// 仮：ワイヤーフレームの線で接続。将来的には伸縮する3Dモデル（立方体）に差し替える
	if (hasClone_) {
		PrimitiveDrawer::GetInstance()->SetCamera(&camera_);
		PrimitiveDrawer::GetInstance()->DrawLine3d(player_->GetWorldTransform().translation_, cloneWorldTransform_.translation_, {1.0f, 1.0f, 1.0f, 1.0f});
	}
}

void GameScene::GenerateBlocks() {
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
			
			case MapChipType::kBlank:
			default:
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
		}
	}
}

///// ----- クローン ----- /////
/// --- 生成 ---
void GameScene::SpawnClone() {
	// クローン用ワールドトランスフォームの初期化
	cloneWorldTransform_.Initialize();

	// 自機の位置を基準に、X方向にオフセットした位置へ配置
	cloneWorldTransform_.translation_ = player_->GetWorldTransform().translation_;
	cloneWorldTransform_.translation_.x += kCloneOffsetX;

	hasClone_ = true;
}
