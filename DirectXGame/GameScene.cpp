#include "GameScene.h"
#include "2d/ImGuiManager.h"
#include "CollisionUtility.h"
#include "WorldTransformConfig.h"
#include <cassert>
#include "math/MathUtility.h"
#include <cmath> // std::abs
#include <map>
#include <string>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility; // 追加

GameScene::GameScene(int stageNumber) : stageNumber_(stageNumber) {}

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
	delete modelCloneBase_;
	delete backgroundSprite_;
	delete mapChipField_;

	delete debugCamera_;

	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			delete worldTransformBlock;
		}
	}
	worldTransformBlocks_.clear();

	// クローンの素の解放
	for (CloneBase* cloneBase : cloneBases_) {
		delete cloneBase;
	}
	cloneBases_.clear();
}

void GameScene::Initialize() {

	// プレイヤーモデル生成
	modelPlayer_ = Model::CreateFromOBJ("player", true);
	// ブロックモデル生成
	modelBlock_ = Model::CreateFromOBJ("block", true);
	// レーザーモデル生成
	modelLazer_ = Model::CreateFromOBJ("Lazer", true);
	// 水モデルの生成
	modelWater_ = Model::CreateFromOBJ("water", true);
	modelWater_->SetAlpha(0.4f);
	// 天球のモデル生成
	modelSkydome_ = Model::CreateFromOBJ("Skydome", true);
	// クローンの素モデル生成（球体）
	modelCloneBase_ = Model::CreateSphere();
	// 背景スプライトの生成
	backgroundTextureHandle_ = TextureManager::Load("uvChecker.png");
	backgroundSprite_ = Sprite::Create(backgroundTextureHandle_, {0.0f, 0.0f});
	backgroundSprite_->SetSize({1280.0f, 720.0f});

	// マップチップフィールドの初期化と生成
	std::string mapPath = "Resources/map/map_" + std::to_string(stageNumber_) + ".csv";

	mapChipField_ = new MapChipField();
	mapChipField_->LoadMapChipCsv(mapPath);

	// カメラの初期化
	camera_.farZ = 1000.0f;
	camera_.Initialize();

	// CSVの配置情報からブロックとプレイヤーを生成
	GenerateBlocks();
	assert(player_ != nullptr && "player is not placed in map.csv");
	line3D_ = new Line3D();
	line3D_->Initialize();

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
	if (controlledClone_ && Input::GetInstance()->IsTriggerMouse(1)) {
		controlledClone_ = nullptr;
		cameraController_->SetTarget(player_);
	}

	// 現在操作しているキャラクター（通常は自機、クローンを操作中はそのクローンの中のPlayer）
	Player* activePlayer = controlledClone_ ? controlledClone_->GetPlayer() : player_;

	// クローンの素を「障害物」として扱うための矩形一覧を作る（持っている素は除く）
	std::vector<MapChipField::Rect> cloneBaseRects;
	for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase == heldCloneBase_) {
			continue;
		}
		cloneBaseRects.push_back(cloneBase->GetRect());
	}

	// プレイヤーの更新
	bool isTryingToFire = player_->IsOnGround() && !line3D_->IsActive() && Input::GetInstance()->IsTriggerMouse(0);
	bool canActivePlayerMove = !line3D_->IsActive() && !isTryingToFire;
	player_->Update(controlledClone_ == nullptr && canActivePlayerMove, cloneBaseRects);

	// レーザーの更新
	for (Lazer* lazer : lazers_) {
		lazer->Update();
	}

	// 水の更新
	for (auto& line : worldTransformWaters_) {
		for (WorldTransform* water : line) {
			if (water) {
				UpdateWorldTransform(*water);
			}
		}
	}

	// 全ての当たり判定を行う
	CheckAllCollisions();

	// プレイヤーとクローンの素の当たり判定、スペースキーで持つ処理（仮実装）
	UpdateCloneBasePickup();

	// クローンの素の更新（複数配置に対応）
	for (CloneBase* cloneBase : cloneBases_) {
		cloneBase->Update(cloneBase == controlledClone_ && canActivePlayerMove, cloneBaseRects);
	}

	// ImGui上でクローンの素の配置・状態を管理するパネルを表示する
	ShowCloneBaseManagerImGui();

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
	if (Input::GetInstance()->TriggerKey(DIK_P)) {
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

	line3D_->Update(activePlayer->GetWorldTransform().translation_, camera_, mapChipField_, activePlayer->IsOnGround(), controlledClone_ != nullptr);

	if (controlledClone_ == nullptr && line3D_->IsActive() && !line3D_->IsCloneLine()) {
		for (CloneBase* cloneBase : cloneBases_) {
			if (line3D_->IsTouchingSphere(cloneBase->GetWorldTransform().translation_, CloneBase::kCollisionRadius)) {
				cloneBase->Transform();
				controlledClone_ = cloneBase;
				cameraController_->SetTarget(cloneBase->GetPlayer());
				break;
			}
		}
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

	// クローンの素の描画（球体、または線接続後は自機と同じ形）
	for (CloneBase* cloneBase : cloneBases_) {
		cloneBase->Draw();
	}

	// プレイヤーの描画
	player_->Draw();

	// 実際の線と予測線の描画
	line3D_->Draw(camera_);

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

		// 水の描画
	for (auto& line : worldTransformWaters_) {
		for (WorldTransform* water : line) {
			if (water) {
				modelWater_->Draw(*water, camera_);
			}
		}
	}

	Model::PostDraw();
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

	// サブIDごとにレーザーの座標を格納する
	std::map<uint8_t, std::vector<Vector3>> lazerPositionsByID;

	worldTransformWaters_.resize(numBlockVertical);
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		worldTransformWaters_[i].resize(numBlockHorizontal, nullptr);
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
				player_->Initialize(modelPlayer_, &camera_, playerPosition);
				player_->SetMapChipField(mapChipField_);
				break;
			}
			case MapChipType::kLazer: {
				uint8_t subID = mapChipField_->GetMapChipSubIDByIndex(j, i);
				lazerPositionsByID[subID].push_back(mapChipField_->GetMapChipPositionByIndex(j, i));
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
			case MapChipType::kWater: {
				worldTransformWaters_[i][j] = new WorldTransform();
				worldTransformWaters_[i][j]->Initialize();
				worldTransformWaters_[i][j]->translation_ = mapChipField_->GetMapChipPositionByIndex(j, i);

				UpdateWorldTransform(*worldTransformWaters_[i][j]);
				break;
			}
			case MapChipType::kCloneBase: {
				// クローンの素を生成（CSV上の "C0"。複数配置に対応）
				Vector3 cloneBasePosition = mapChipField_->GetMapChipPositionByIndex(j, i);
				CloneBase* cloneBase = new CloneBase();
				cloneBase->Initialize(modelCloneBase_, modelPlayer_, &camera_, mapChipField_, cloneBasePosition);
				cloneBases_.push_back(cloneBase);
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

///// ----- クローンの素 ----- /////
/// --- ImGui管理パネル ---
// NOTE: ImGuiの表示文字列は日本語だと文字化けするため、英語表記にしている（コード上のコメントは日本語のままでよい）
void GameScene::ShowCloneBaseManagerImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("CloneBase Manager");

	// 当たり判定・拾えるかどうかの状態（仮実装）
	ImGui::Text("Colliding With CloneBase: %s", isCollidingWithCloneBase_ ? "True" : "False");
	ImGui::Text("Can Pick Up: %s", canPickUpCloneBase_ ? "True" : "False");
	ImGui::Text("Holding CloneBase: %s", isHoldingCloneBase_ ? "True" : "False");
	ImGui::Separator();

	if (cloneBases_.empty()) {
		// まだ1体も配置されていない場合
		ImGui::Text("No clone bases placed");
	} else {
		for (size_t i = 0; i < cloneBases_.size(); ++i) {
			CloneBase* cloneBase = cloneBases_[i];
			const Vector3& pos = cloneBase->GetWorldTransform().translation_;
			bool isTransformed = cloneBase->GetState() == CloneBase::State::kTransformed;
			const char* stateText = isTransformed ? "Transformed (Line Connected)" : "Base (Not Connected)";

			ImGui::PushID(static_cast<int>(i));
			ImGui::Text("C0[%zu] Pos:(%.1f, %.1f, %.1f) State:%s%s", i, pos.x, pos.y, pos.z, stateText, cloneBase->IsHeld() ? " [Held]" : "");
			ImGui::SameLine();
			if (isTransformed) {
				if (ImGui::Button("Reset To Base (Debug)")) {
					cloneBase->ResetToBase();
				}
			} else {
				if (ImGui::Button("Connect Line (Debug)")) {
					cloneBase->Transform();
				}
			}
			ImGui::PopID();
		}
	}

	ImGui::End();
#endif
}

///// ----- クローンの素を持つ処理（仮実装） ----- /////
// プレイヤーとクローンの素（球体）の当たり判定を取り、当たっている間にスペースキーを押すと持つ。
// 今は「触れていたら拾える」実装。将来的には自機を中心とした円の半径内なら拾えるようにする予定。
void GameScene::UpdateCloneBasePickup() {
	// 前フレームの向きを読み取ってから、今の向きで上書きしておく
	Player::LRDirection previousDirection = previousPlayerDirection_;
	previousPlayerDirection_ = player_->GetLRDirection();

	isCollidingWithCloneBase_ = false;
	canPickUpCloneBase_ = false;

	const Vector3& playerPos = player_->GetWorldTransform().translation_;
	float playerHalfWidth = player_->GetWidth() / 2.0f;
	float playerHalfHeight = player_->GetHeight() / 2.0f;

	// クローンの素を持っている間は、プレイヤーの正面に隙間なくくっつける
	if (isHoldingCloneBase_ && heldCloneBase_) {
		bool directionChanged = (player_->GetLRDirection() != previousDirection);

		Vector3 targetPosition = ComputeHeldCloneBasePosition();

		// 方向転換した先にブロックがあるなら、旋回をキャンセルして元の向きに戻す
		if (directionChanged && heldCloneBase_->IsCollidingWithBlock(targetPosition, mapChipField_)) {
			player_->CancelTurn();
			previousPlayerDirection_ = player_->GetLRDirection(); // 戻した向きを記録し直す
			targetPosition = ComputeHeldCloneBasePosition();      // 戻した向きで座標を再計算
		}

		heldCloneBase_->SetTranslation(targetPosition);
		return;
	}

	// (以降、当たり判定と「拾う」入力を確認する処理は変更なし)
	for (CloneBase* cloneBase : cloneBases_) {
		bool isColliding =
		    CollisionUtility::IsCollisionBoxAndSphere(playerPos, playerHalfWidth, playerHalfHeight, playerHalfWidth, cloneBase->GetWorldTransform().translation_, CloneBase::kCollisionRadius);

		if (!isColliding) {
			continue;
		}

		isCollidingWithCloneBase_ = true;
		canPickUpCloneBase_ = true;

		if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			isHoldingCloneBase_ = true;
			heldCloneBase_ = cloneBase;
			cloneBase->PickUp();
		}

		break;
	}
}

///// ----- 全ての当たり判定を行う ----- /////
void GameScene::CheckAllCollisions() {
	/// --- 自キャラとクローンの素の当たり判定(拾える判定) ---
	{
		isCollidingWithCloneBase_ = false;
		canPickUpCloneBase_ = false;
		collidingCloneBase_ = nullptr; // 今どのクローンの素と当たっているか（新規に用意する）

		const Vector3& playerPos = player_->GetWorldTransform().translation_;
		float playerHalfWidth = player_->GetWidth() / 2.0f;
		float playerHalfHeight = player_->GetHeight() / 2.0f;

		for (CloneBase* cloneBase : cloneBases_) {
			bool isColliding =
			    CollisionUtility::IsCollisionBoxAndSphere(playerPos, playerHalfWidth, playerHalfHeight, playerHalfWidth, cloneBase->GetWorldTransform().translation_, CloneBase::kCollisionRadius);

			if (!isColliding) {
				continue;
			}

			isCollidingWithCloneBase_ = true;
			canPickUpCloneBase_ = true;
			collidingCloneBase_ = cloneBase;
			break; // 仮実装として最初に当たった1体だけを対象にする
		}
	}
}

///// ----- 持っているクローンの素の追従位置を計算する ----- /////
Vector3 GameScene::ComputeHeldCloneBasePosition() const {
	const Vector3& playerPos = player_->GetWorldTransform().translation_;
	float playerHalfWidth = player_->GetWidth() / 2.0f;
	float cloneHalfWidth = heldCloneBase_->GetWidth() / 2.0f;

	// 向いている方向 (+1: 右, -1: 左)
	float direction = (player_->GetLRDirection() == Player::LRDirection::kRight) ? 1.0f : -1.0f;

	// 自機の正面に、隙間なくくっつく位置
	float offsetX = direction * (playerHalfWidth + cloneHalfWidth);

	return playerPos + Vector3(offsetX, 0.0f, 0.0f);
}

///// ----- 当たり判定用の矩形を取得する ----- /////
MapChipField::Rect CloneBase::GetRect() const {
	const Vector3& pos = worldTransform_.translation_;
	MapChipField::Rect rect;
	rect.left = pos.x - kWidth / 2.0f;
	rect.right = pos.x + kWidth / 2.0f;
	rect.bottom = pos.y - kHeight / 2.0f;
	rect.top = pos.y + kHeight / 2.0f;
	return rect;
}
