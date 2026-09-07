#include "GameScene.h"
#include "2d/ImGuiManager.h"
#include "CollisionUtility.h"
#include "WorldTransformConfig.h"
#include "math/MathUtility.h"
#include <cassert>
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
	for (PushPlate* plate : pressurePlates_) {
		delete plate;
	}
	pressurePlates_.clear();

	for (Door* door : doors_) {
		delete door;
	}
	doors_.clear();

	for (Key* key : keys_) {
		delete key;
	}
	keys_.clear();

	delete debugCamera_;
	delete mouseCursor_;
	delete throwAimIndicator_;

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

	// マウスカーソル表示（AL3_評価課題02から流用）
	mouseCursor_ = new MouseCursor();
	mouseCursor_->Initialize(&camera_);

	// クローンの素を持っている間だけ表示する、投げる方向を示すUI（円＋三角形）
	throwAimIndicator_ = new ThrowAimIndicator();
	throwAimIndicator_->Initialize(&camera_);
}

void GameScene::Update() {
	// マウスカーソルの更新（投げる方向の計算、表示に使用）
	if (mouseCursor_ != nullptr) {
		mouseCursor_->Update();
	}

	// クローンの素を持っている間だけ、投げる方向を示すUI（円＋三角形）を更新する
	if (isHoldingCloneBase_ && throwAimIndicator_ != nullptr) {
		throwAimIndicator_->Update(player_->GetWorldTransform().translation_, mouseCursor_->GetWorldPosition());
	}
#ifdef USE_IMGUI
	ImGui::Begin("Background");
	ImGui::RadioButton("Skydome", &backgroundMode_, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Sprite", &backgroundMode_, 1);
	ImGui::End();
#endif
	if (controlledClone_ && Input::GetInstance()->IsTriggerMouse(1)) {
		// 自機とのリンクを切ったら、クローンの素（球体）に戻す
		// （現在位置を引き継ぎ、空中なら重力で落下を再開する）
		controlledClone_->ResetToBase();
		controlledClone_ = nullptr;
		cameraController_->SetTarget(player_);
	}

	// 現在操作しているキャラクター（通常は自機、クローンを操作中はそのクローンの中のPlayer）
	Player* activePlayer = controlledClone_ ? controlledClone_->GetPlayer() : player_;
	UpdatePressurePlates();
	UpdateDoors();
	UpdateLazers();
	
	// クローンの素を「障害物」として扱うための矩形一覧を作る（持っている素は除く）
	std::vector<MapChipField::Rect> cloneBaseRects;
	// 変形済みクローンの矩形一覧（自機や他のクローンとの当たり判定にだけ使う。素・ドアとは別枠にしておく）
	std::vector<MapChipField::Rect> transformedCloneRects;
	for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase == heldCloneBase_) {
			continue;
		}

		// 変身済みクローンは球体の障害物としてではなく、自機と同じ形の障害物として別枠で扱う
		if (cloneBase->GetState() == CloneBase::State::kTransformed) {
			transformedCloneRects.push_back(cloneBase->GetRect());
			continue;
		}

		cloneBaseRects.push_back(cloneBase->GetRect());
	}

	// 閉じているドアを障害物かつ接続線の反射面として扱う。
	std::vector<MapChipField::Rect> closedDoorRects;
	for (const Door* door : doors_) {
		if (door->IsOpen()) {
			continue;
		}
		const MapChipField::Rect doorRect = door->GetRect();
		cloneBaseRects.push_back(doorRect);
		closedDoorRects.push_back(doorRect);
	}

	// レーザーは通常プレイヤーだけを止める。クローンへ渡す障害物一覧には追加しない。
	std::vector<MapChipField::Rect> activeLazerRects;
	// 自機の障害物一覧（素・ドア・変形済みクローン・レーザー）
	std::vector<MapChipField::Rect> playerObstacleRects = cloneBaseRects;
	playerObstacleRects.insert(playerObstacleRects.end(), transformedCloneRects.begin(), transformedCloneRects.end());
	for (const Lazer* lazer : lazers_) {
		if (lazer->IsActive()) {
			const MapChipField::Rect rect = lazer->GetRect();
			activeLazerRects.push_back(rect);
			playerObstacleRects.push_back(rect);
		}
	}

	// クローンの素を持っている間は、自機の当たり判定の横幅を広げる（ImGuiのHolding Widthで調整可能）
	player_->SetIsHolding(isHoldingCloneBase_);

	// プレイヤーの更新
	// クローンの素を持っている間はリンク線を発射できないようにする
	bool isTryingToFire = player_->IsOnGround() && !line3D_->IsActive() && !isHoldingCloneBase_ && Input::GetInstance()->IsTriggerMouse(0);
	bool canActivePlayerMove = !line3D_->IsActive() && !isTryingToFire;
	player_->Update(controlledClone_ == nullptr && canActivePlayerMove, playerObstacleRects);

	UpdateKeys(player_);
	UpdateDoors();
	CheckDoorGoal(player_);

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

	// 自機の当たり判定矩形（投げたクローンの素が自機の上に乗れるようにするため）
	MapChipField::Rect playerRect;
	playerRect.left = player_->GetWorldTransform().translation_.x - player_->GetWidth() / 2.0f;
	playerRect.right = player_->GetWorldTransform().translation_.x + player_->GetWidth() / 2.0f;
	playerRect.bottom = player_->GetWorldTransform().translation_.y - player_->GetHeight() / 2.0f;
	playerRect.top = player_->GetWorldTransform().translation_.y + player_->GetHeight() / 2.0f;

	// クローンの素の更新（複数配置に対応）
	for (CloneBase* cloneBase : cloneBases_) {
		// この素専用の障害物一覧：共通の素・ドアに加えて、自機と「自分以外」の変形済みクローンを含める
		// （自機と変形後クローンの当たり判定、クローン同士の当たり判定を成立させるため）
		std::vector<MapChipField::Rect> obstacleRectsForClone = cloneBaseRects;
		obstacleRectsForClone.push_back(playerRect);
		for (CloneBase* other : cloneBases_) {
			if (other == cloneBase || other->GetState() != CloneBase::State::kTransformed) {
				continue;
			}
			obstacleRectsForClone.push_back(other->GetRect());
		}

		cloneBase->Update(cloneBase == controlledClone_ && canActivePlayerMove, obstacleRectsForClone, playerRect);

		if (cloneBase->ConsumeWaterDestroyed()) {
			// 消滅したクローンを操作していた場合
			if (controlledClone_ == cloneBase) {
				controlledClone_ = nullptr;

				// 接続線を切る
				line3D_->ResetLine();

				// カメラを通常プレイヤーへ戻す
				cameraController_->SetTarget(player_);
			}
		}
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

	// クローンの素を持っている間はリンク線を発射できないようにする
	bool canFireLine = activePlayer->IsOnGround() && !isHoldingCloneBase_;
	line3D_->Update(
	    activePlayer->GetWorldTransform().translation_, camera_, mapChipField_, closedDoorRects, activeLazerRects,
	    canFireLine, controlledClone_ != nullptr);

	if (controlledClone_ == nullptr && line3D_->IsActive() && !line3D_->IsCloneLine()) {

		for (CloneBase* cloneBase : cloneBases_) {
			if (line3D_->IsTouchingSphere(cloneBase->GetWorldTransform().translation_, CloneBase::kCollisionRadius)) {

				cloneBase->Transform();
				controlledClone_ = cloneBase;
				cameraController_->SetTarget(cloneBase->GetPlayer());

				// クローンに当たった線を消す
				line3D_->ResetLine();

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

	// 感圧板の描画
	for (PushPlate* plate : pressurePlates_) {
		plate->Draw();
	}

	for (Key* key : keys_) {
		key->Draw();
	}

	// 扉の描画
	for (Door* door : doors_) {
		door->Draw();
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

	// 3Dモデルより手前へマウスカーソルの円を描画する
	if (mouseCursor_ != nullptr) {
		mouseCursor_->Draw();
	}

	// クローンの素を持っている間だけ、投げる方向を示すUI（円＋三角形）を描画する
	if (isHoldingCloneBase_ && throwAimIndicator_ != nullptr) {
		throwAimIndicator_->Draw();
	}

	// 自機の当たり判定サイズを可視化するワイヤーフレーム（Debugビルド/USE_IMGUIの時だけ表示）
	DrawPlayerCollisionWireframe();
}

///// ----- 自機の当たり判定サイズを可視化するワイヤーフレーム ----- /////
// ImGuiパネルと同じ扱い（USE_IMGUIが定義されるDebugビルドの時だけ表示される。Releaseでは何もしない）
void GameScene::DrawPlayerCollisionWireframe() {
#ifdef USE_IMGUI
	const Vector3& pos = player_->GetWorldTransform().translation_;
	float halfWidth = player_->GetWidth() / 2.0f;
	float halfHeight = player_->GetHeight() / 2.0f;
	// 奥行き(Z)は実際の当たり判定には使っていないため、見た目を立方体に近づけるための仮の値として高さと同じにする
	float halfDepth = halfHeight;

	Vector3 corners[8] = {
	    {pos.x - halfWidth, pos.y - halfHeight, pos.z - halfDepth}, {pos.x + halfWidth, pos.y - halfHeight, pos.z - halfDepth},
	    {pos.x + halfWidth, pos.y + halfHeight, pos.z - halfDepth}, {pos.x - halfWidth, pos.y + halfHeight, pos.z - halfDepth},
	    {pos.x - halfWidth, pos.y - halfHeight, pos.z + halfDepth}, {pos.x + halfWidth, pos.y - halfHeight, pos.z + halfDepth},
	    {pos.x + halfWidth, pos.y + halfHeight, pos.z + halfDepth}, {pos.x - halfWidth, pos.y + halfHeight, pos.z + halfDepth},
	};

	// 持っている間は色を変えて、今どちらの状態かひと目でわかるようにする
	Vector4 color = player_->IsHolding() ? Vector4{1.0f, 0.5f, 0.0f, 1.0f} : Vector4{0.0f, 1.0f, 1.0f, 1.0f};

	PrimitiveDrawer* drawer = PrimitiveDrawer::GetInstance();
	drawer->SetCamera(&camera_);

	// 手前の面
	drawer->DrawLine3d(corners[0], corners[1], color);
	drawer->DrawLine3d(corners[1], corners[2], color);
	drawer->DrawLine3d(corners[2], corners[3], color);
	drawer->DrawLine3d(corners[3], corners[0], color);
	// 奥の面
	drawer->DrawLine3d(corners[4], corners[5], color);
	drawer->DrawLine3d(corners[5], corners[6], color);
	drawer->DrawLine3d(corners[6], corners[7], color);
	drawer->DrawLine3d(corners[7], corners[4], color);
	// 手前と奥をつなぐ辺
	drawer->DrawLine3d(corners[0], corners[4], color);
	drawer->DrawLine3d(corners[1], corners[5], color);
	drawer->DrawLine3d(corners[2], corners[6], color);
	drawer->DrawLine3d(corners[3], corners[7], color);
#endif
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
			case MapChipType::kPushPlate: {
				const uint8_t plateID = mapChipField_->GetMapChipSubIDByIndex(j, i);
				const uint8_t requiredCount = mapChipField_->GetRequiredActorCountByIndex(j, i);

				// 左隣が同じ設定なら、左端の処理ですでにまとめて生成されている。
				if (j > 0 && mapChipField_->GetMapChipTypeByIndex(j - 1, i) == MapChipType::kPushPlate && mapChipField_->GetMapChipSubIDByIndex(j - 1, i) == plateID &&
				    mapChipField_->GetRequiredActorCountByIndex(j - 1, i) == requiredCount) {
					worldTransformBlocks_[i][j] = nullptr;
					break;
				}

				// 同じ行に連続する、同じID・同じ必要人数の感圧板の右端を探す。
				uint32_t endX = j;
				while (endX + 1 < numBlockHorizontal && mapChipField_->GetMapChipTypeByIndex(endX + 1, i) == MapChipType::kPushPlate && mapChipField_->GetMapChipSubIDByIndex(endX + 1, i) == plateID &&
				       mapChipField_->GetRequiredActorCountByIndex(endX + 1, i) == requiredCount) {
					++endX;
				}

				const Vector3 leftPosition = mapChipField_->GetMapChipPositionByIndex(j, i);
				const Vector3 rightPosition = mapChipField_->GetMapChipPositionByIndex(endX, i);
				Vector3 centerPosition = leftPosition;
				centerPosition.x = (leftPosition.x + rightPosition.x) / 2.0f;
				const float plateWidth = static_cast<float>(endX - j + 1) * MapChipField::kBlockWidth;

				PushPlate* plate = new PushPlate();
				plate->Initialize(modelBlock_, &camera_, centerPosition, plateID, requiredCount, plateWidth);
				pressurePlates_.push_back(plate);
				for (uint32_t plateX = j; plateX <= endX; ++plateX) {
					worldTransformBlocks_[i][plateX] = nullptr;
				}
				break;
			}
			case MapChipType::kDoor: {
				Door* door = new Door();
				door->Initialize(modelBlock_, &camera_, mapChipField_->GetMapChipPositionByIndex(j, i), mapChipField_->GetMapChipSubIDByIndex(j, i));
				doors_.push_back(door);
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
			case MapChipType::kKey: {
				Key* key = new Key();

				key->Initialize(
					modelCloneBase_,
					&camera_,
					mapChipField_->GetMapChipPositionByIndex(j, i),
					mapChipField_->GetMapChipSubIDByIndex(j, i));

				keys_.push_back(key);
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
		lazer->Initialize(modelLazer_, &camera_, positions.front(), positions.back(), subID);
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

	// 自機の当たり判定が今どちらのサイズになっているか（Normal / Holding）を表示する
	ImGui::Text("Player Hitbox Mode: %s (Width: %.2f)", player_->IsHolding() ? "Holding" : "Normal", player_->GetWidth());
	// 持っている間の当たり判定の横幅を調整する（縦方向はkHeightのまま変えない）
	ImGui::SliderFloat("Holding Width", &player_->GetHoldingWidthRef(), 1.0f, 2.0f);
	ImGui::Separator();

	// 投げる力を調整する（距離に関わらず常にこの力で投げる）
	ImGui::SliderFloat("Throw Power", &throwPower_, 0.0f, kMaxThrowPower);

	// 投げる方向を示すUI（円＋三角形）の大きさを調整する
	if (throwAimIndicator_ != nullptr) {
		ImGui::SliderFloat("Aim Circle Radius", &throwAimIndicator_->circleRadius_, 0.5f, 5.0f);
		ImGui::SliderFloat("Aim Triangle Size", &throwAimIndicator_->triangleSize_, 10.0f, 150.0f);
	}
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
		// 持っている間にスペースキーを押したら、マウスカーソル方向へ投げる
		if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			ThrowHeldCloneBase();
			return;
		}

		bool directionChanged = (player_->GetLRDirection() != previousDirection);

		Vector3 targetPosition = ComputeHeldCloneBasePosition();

		// 方向転換した先にブロックがあるなら、旋回をキャンセルして元の向きに戻す
		if (directionChanged && heldCloneBase_->IsCollidingWithBlock(targetPosition, mapChipField_)) {
			player_->CancelTurn();
			previousPlayerDirection_ = player_->GetLRDirection(); // 戻した向きを記録し直す
			targetPosition = ComputeHeldCloneBasePosition();      // 戻した向きで座標を再計算
		}

		// ブロックにめり込まないよう、当たり判定を取りながら目標位置へ移動させる
		heldCloneBase_->MoveHeldTo(targetPosition);
		return;
	}

	// (以降、当たり判定と「拾う」入力を確認する処理は変更なし)
	for (CloneBase* cloneBase : cloneBases_) {

		if (cloneBase->GetState() == CloneBase::State::kTransformed) {
			continue;
		}

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

///// ----- クローンの素を投げる処理 ----- /////
// 円周上の三角形がとがっている方向（ThrowAimIndicator）へ、持っているクローンの素を投げる
void GameScene::ThrowHeldCloneBase() {
	// 三角形の向き（＝自機からマウスカーソルへ向かう方向）をそのまま投げる方向にする
	Vector3 direction = throwAimIndicator_->GetThrowDirection();

	// 力はカーソルまでの距離に関係なく、常に一定（ImGuiのThrowPowerで調整）
	heldCloneBase_->Throw(direction * throwPower_);
	heldCloneBase_->Release();

	isHoldingCloneBase_ = false;
	heldCloneBase_ = nullptr;
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

			if (cloneBase->GetState() == CloneBase::State::kTransformed) {
				continue;
			}

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
	// 持っている間の当たり判定拡張(GetWidth())とは切り離し、見た目の追従位置は常に通常幅を基準にする
	float playerHalfWidth = Player::GetNormalWidth() / 2.0f;
	float cloneHalfWidth = heldCloneBase_->GetWidth() / 2.0f;

	// 向いている方向 (+1: 右, -1: 左)
	float direction = (player_->GetLRDirection() == Player::LRDirection::kRight) ? 1.0f : -1.0f;

	// 自機の正面に、隙間なくくっつく位置
	float offsetX = direction * (playerHalfWidth + cloneHalfWidth);

	return playerPos + Vector3(offsetX, 0.0f, 0.0f);
}

void GameScene::UpdatePressurePlates() {
	// 本体プレイヤー
	std::vector<Player*> actors = {player_};

	 // 変身前のクローンの素
	std::vector<MapChipField::Rect> cloneBaseRects;

	 for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase->GetState() == CloneBase::State::kTransformed) {

			// 変身済みならPlayerとして数える
			actors.push_back(cloneBase->GetPlayer());

		} else if (!cloneBase->IsHeld()) {

			// 持っていないクローンの素だけ数える
			cloneBaseRects.push_back(cloneBase->GetRect());
		}
	}

	for (PushPlate* plate : pressurePlates_) {
		plate->Update(actors, cloneBaseRects);
	}
}

void GameScene::UpdateLazers() {
	for (Lazer* lazer : lazers_) {
		bool shouldDisable = false;
		for (const PushPlate* plate : pressurePlates_) {
			if (plate->GetID() == lazer->GetID() && plate->IsPushed()) {
				shouldDisable = true;
				break;
			}
		}
		lazer->SetActive(!shouldDisable);
	}
}

void GameScene::UpdateKeys(Player* activePlayer) {
	for (Key* key : keys_) {
		key->Update(activePlayer);
	}
}

void GameScene::UpdateDoors() {
	for (Door* door : doors_) {
		bool shouldOpen = false;

		for (const Key* key : keys_) {
			if (key->GetID() == door->GetID() && key->IsCollected()) {

				shouldOpen = true;
				break;
			}
		}

		door->SetOpen(shouldOpen);
		door->Update();
	}
}

void GameScene::CheckDoorGoal(const Player* activePlayer) {

	for (const Door* door : doors_) {
		if (door->IsOpen() && door->IsCollidingWithPlayer(activePlayer)) {

			isFinished_ = true;
			return;
		}
	}
}