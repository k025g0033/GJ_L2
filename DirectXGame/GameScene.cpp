#include "GameScene.h"
#include "AudioSettings.h"
#include "2d/ImGuiManager.h"
#include "CollisionUtility.h"
#include "WorldTransformConfig.h"
#include "math/MathUtility.h"
#include <algorithm>
#include <cassert>
#include <cmath> // std::abs
#include <map>
#include <string>

namespace {

bool IsRectColliding(const MapChipField::Rect& a, const MapChipField::Rect& b) { return a.right > b.left && a.left < b.right && a.top > b.bottom && a.bottom < b.top; }

bool IsStandingOnRect(const MapChipField::Rect& actor, const MapChipField::Rect& platform) {
	constexpr float kStandingTolerance = 0.12f;
	const bool overlapsX = actor.right > platform.left && actor.left < platform.right;
	const bool isNearTop = actor.bottom >= platform.top - kStandingTolerance && actor.bottom <= platform.top + kStandingTolerance;
	return overlapsX && isNearTop;
}

// 指定した分だけ矩形を四方に広げる
MapChipField::Rect ExpandRect(const MapChipField::Rect& rect, float margin) { return {rect.left - margin, rect.right + margin, rect.bottom - margin, rect.top + margin}; }

// 帯電の受け渡しに使う接触判定のあそび。
// クローン同士は当たり判定で押し戻されるため、矩形がぴったり重なることは絶対にない。
// 少しだけ広げた矩形で判定して、隣り合っていれば「接触している」とみなす。
const float kChargeContactMargin = 0.1f;

float SmoothStep(float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

KamataEngine::Vector3 LerpVector3(
    const KamataEngine::Vector3& start, const KamataEngine::Vector3& end, float t) {
	return {
	    start.x + (end.x - start.x) * t,
	    start.y + (end.y - start.y) * t,
	    start.z + (end.z - start.z) * t,
	};
}

// ステージごとのカメラ設定ファイル
const char* kCameraSettingsCsvPath = "Resources/map/camera.csv";
} // namespace

using namespace KamataEngine;
using namespace KamataEngine::MathUtility; // 追加

GameScene::GameScene(int stageNumber, bool useTitleMap)
    : stageNumber_(stageNumber), useTitleMap_(useTitleMap) {}

GameScene::~GameScene() {
	// 解放
	delete modelPlayer_;
	delete modelBlock_;
	delete modelElectricPlatform_;
	delete modelChargePoint_;
	delete modelDoor_;
	delete modelDoorOpen_;
	delete modelDoorOpenGlass_;
	delete modelPushPlateBase_;
	delete modelPushPlateButton_;
	delete modelSkydome_;
	delete modelWater_;
	for (Lazer* lazer : lazers_) {
		delete lazer;
	}
	lazers_.clear();
	delete modelLazer_;
	delete modelCloneBase_;
	delete modelKey_;
	delete modelPlayerHead_;
	delete modelPlayerLeftArm_;
	delete modelPlayerRightArm_;
	delete modelPlayerLeftArmHolding_;
	delete modelPlayerRightArmHolding_;
	delete modelPlayerHoldingClone_;
	delete background_;
	delete pauseEscSprite_;
	delete pausePoseGuideSprite_;
	delete pauseOverlaySprite_;
	delete settingsCloseSprite_;
	delete settingsBgmSprite_;
	delete settingsSeSprite_;
	delete settingsPredictionSprite_;
	delete settingsPredictionOnSprite_;
	delete settingsPredictionOffSprite_;
	delete settingsSeVolumeBarSprite_;
	delete settingsBgmVolumeBarSprite_;
	for (Sprite* sprite : pauseMenuSprites_) {
		delete sprite;
	}
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

	for (ElectricBullet* bullet : electricBullets_) {
		delete bullet;
	}
	electricBullets_.clear();
	delete modelElectricBullet_;

	for (ChargePoint* chargePoint : chargePoints_) {
		delete chargePoint;
	}
	chargePoints_.clear();

	for (ElectricPlatform* platform : electricPlatforms_) {
		delete platform;
	}
	electricPlatforms_.clear();

	delete debugCamera_;
	delete mouseCursor_;
	delete throwAimIndicator_;

	for (std::vector<WorldTransform*>& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			delete worldTransformBlock;
		}
	}
	worldTransformBlocks_.clear();
	for (std::vector<WorldTransform*>& waterLine : worldTransformWaters_) {
		for (WorldTransform* water : waterLine) {
			delete water;
		}
	}
	worldTransformWaters_.clear();

	// クローンの素の解放
	for (CloneBase* cloneBase : cloneBases_) {
		delete cloneBase;
	}
	cloneBases_.clear();
}

void GameScene::Initialize() {

	// 旧プレイヤーモデル（player.obj、立方体）。
	// 自機もクローンもパーツ構成に切り替えたので、今はどこからも使っていない。
	// クローン専用の1体モデルを用意したら、ここで読み込んでCloneBase::Initializeへ渡す。
	// modelPlayer_ = Model::CreateFromOBJ("player", true);
	// ブロックモデル生成
	modelBlock_ = Model::CreateFromOBJ("block", true);
	// 電動足場専用モデル
	modelElectricPlatform_ = Model::CreateFromOBJ("ElecPlate", true);
	// エネルギー源専用モデル
	modelChargePoint_ = Model::CreateFromOBJ("ChargePoint", true);
	// ドア専用モデル
	modelDoor_ = Model::CreateFromOBJ("Door", true);
	modelDoorOpen_ = Model::CreateFromOBJ("Door_open", true);
	// 板ガラスはマテリアルが別なので、モデルも分けて重ねて描く
	modelDoorOpenGlass_ = Model::CreateFromOBJ("Door_openPlassObject", true);
	// 感圧板の土台と、上下する押下部分
	modelPushPlateBase_ = Model::CreateFromOBJ("PushPlateBase", true);
	modelPushPlateButton_ = Model::CreateFromOBJ("PushPlateButton", true);
	// レーザーモデル生成
	modelLazer_ = Model::CreateFromOBJ("Lazer", true);
	// 水モデルの生成
	modelWater_ = Model::CreateFromOBJ("water", true);
	modelWater_->SetAlpha(0.4f);
	// 天球のモデル生成
	modelSkydome_ = Model::CreateFromOBJ("Skydome", true);
	// クローンの素モデル生成
	modelCloneBase_ = Model::CreateFromOBJ("cloneObject", true);
	// 鍵モデル生成
	modelKey_ = Model::CreateFromOBJ("Key", true);
	// 電気弾モデル生成
	modelElectricBullet_ = KamataEngine::Model::CreateSphere();
	// 自機の追加パーツモデル生成（頭・左腕・右腕。Blender側で原点をワールド原点に合わせてあるので、
	// ベースモデルと同じワールド変換でそのまま描画すれば正しい位置に組み合わさる）
	modelPlayerHead_ = Model::CreateFromOBJ("player_head", true);
	modelPlayerLeftArm_ = Model::CreateFromOBJ("player_leftArm", true);
	modelPlayerRightArm_ = Model::CreateFromOBJ("player_rightArm", true);
	// クローンの素を持っている間だけ使うパーツ（腕を上げた形と、抱えているクローン）
	modelPlayerLeftArmHolding_ = Model::CreateFromOBJ("player_leftArm_next", true);
	modelPlayerRightArmHolding_ = Model::CreateFromOBJ("player_rightArm_next", true);
	modelPlayerHoldingClone_ = Model::CreateFromOBJ("player_cloneObject_handing", true);

	// 左上のポーズ操作案内と、停止中に表示する画面を生成
	pauseEscTextureHandle_ = TextureManager::Load("Pause/Esc.png");
	pausePoseTextureHandle_ = TextureManager::Load("Pause/Pause.png");
	pauseOverlayTextureHandle_ = TextureManager::Load("white1x1.png");
	pauseEscSprite_ = Sprite::Create(pauseEscTextureHandle_, {8.0f, 8.0f});
	pauseEscSprite_->SetSize({40.0f, 40.0f});
	pausePoseGuideSprite_ = Sprite::Create(pausePoseTextureHandle_, {52.0f, 8.0f});
	pausePoseGuideSprite_->SetSize({40.0f, 40.0f});
	pauseOverlaySprite_ = Sprite::Create(pauseOverlayTextureHandle_, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.65f});
	pauseOverlaySprite_->SetSize({1280.0f, 720.0f});
	settingsCloseTextureHandle_ = TextureManager::Load("Pause/Close.png");
	settingsCloseSprite_ = Sprite::Create(settingsCloseTextureHandle_, {384.0f, 440.0f});
	settingsCloseSprite_->SetSize({512.0f, 64.0f});
	settingsBgmTextureHandle_ = TextureManager::Load("Pause/SettingsBGM.png");
	settingsSeTextureHandle_ = TextureManager::Load("Pause/SettingsSE.png");
	settingsSeSprite_ = Sprite::Create(settingsSeTextureHandle_, {448.0f, 200.0f});
	settingsBgmSprite_ = Sprite::Create(settingsBgmTextureHandle_, {448.0f, 280.0f});
	settingsSeSprite_->SetSize({64.0f, 64.0f});
	settingsBgmSprite_->SetSize({64.0f, 64.0f});
	settingsPredictionTextureHandle_ = TextureManager::Load("Pause/PredictionLine.png");
	settingsPredictionOnTextureHandle_ = TextureManager::Load("Pause/PredictionOn.png");
	settingsPredictionOffTextureHandle_ = TextureManager::Load("Pause/PredictionOff.png");
	settingsPredictionSprite_ = Sprite::Create(settingsPredictionTextureHandle_, {448.0f, 360.0f});
	settingsPredictionOnSprite_ = Sprite::Create(settingsPredictionOnTextureHandle_, {656.0f, 376.0f});
	settingsPredictionOffSprite_ = Sprite::Create(settingsPredictionOffTextureHandle_, {656.0f, 376.0f});
	settingsPredictionSprite_->SetSize({64.0f, 64.0f});
	settingsPredictionOnSprite_->SetSize({32.0f, 32.0f});
	settingsPredictionOffSprite_->SetSize({32.0f, 32.0f});
	settingsVolumeBarTextureHandle_ = TextureManager::Load("Pause/VolumeBar.png");
	settingsSeVolumeBarSprite_ = Sprite::Create(settingsVolumeBarTextureHandle_, {576.0f, 224.0f});
	settingsBgmVolumeBarSprite_ = Sprite::Create(settingsVolumeBarTextureHandle_, {576.0f, 304.0f});
	settingsSeVolumeBarSprite_->SetSize({128.0f, 16.0f});
	settingsBgmVolumeBarSprite_->SetSize({128.0f, 16.0f});
	cursorMoveSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/CursorMove.wav");
	decideSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/Decide.wav");
	electricChargeSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/ElectricCharge.wav");
	electricFireSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/ElectricFire.wav");
	waterSplashSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/WaterSplash.wav");
	keyGetSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/KeyGet.wav");
	pushPlateSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/PushPlate.wav");
	controlSwitchSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/ControlSwitch.wav");
	cloneLandingSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/CloneLanding.wav");
	jumpSoundHandle_ = Audio::GetInstance()->LoadWave("Sound/Jump.wav");

	const std::array<std::string, 4> pauseMenuPaths = {
	    "Pause/Return.png", "Pause/Restart.png", "Pause/StageSelect.png", "Pause/Settings.png"};
	for (int i = 0; i < static_cast<int>(pauseMenuSprites_.size()); ++i) {
		pauseMenuTextureHandles_[i] = TextureManager::Load(pauseMenuPaths[i]);
		pauseMenuSprites_[i] = Sprite::Create(pauseMenuTextureHandles_[i], {512.0f, 240.0f + 80.0f * i});
		pauseMenuSprites_[i]->SetSize({256.0f, 64.0f});
	}

	// マップチップフィールドの初期化と生成
	std::string mapPath = useTitleMap_
	                          ? "Resources/map/map_title.csv"
	                          : "Resources/map/map_" + std::to_string(stageNumber_) + ".csv";

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
	// カメラの生成・初期化
	// ※カメラは自機やクローンには追従しない。ステージごとにCSVで決めた位置に固定する。
	// 　横スクロールを有効にしたステージだけ、自機のX座標に合わせてカメラが横に動く。
	cameraController_ = new CameraController();
	cameraController_->Initialize(&camera_);
	cameraController_->LoadStageSettingsCsv(kCameraSettingsCsvPath);
	cameraController_->SelectStage(stageNumber_);
	// 横スクロールの基準は常に自機（クローンを操作していてもカメラは自機基準のまま）
	cameraController_->SetTarget(player_);
	cameraController_->Reset();
	// カメラと同じステージ番号で、背景画像・配置・雲の設定を選ぶ。
	background_ = new BackGround();
	background_->Initialize(&camera_, stageNumber_);

	// デバッグカメラの生成
	debugCamera_ = new DebugCamera(1280, 720);

	// マウスカーソル表示（AL3_評価課題02から流用）
	mouseCursor_ = new MouseCursor();
	mouseCursor_->Initialize(&camera_);

	// クローンの素を持っている間だけ表示する、投げる方向を示すUI（円＋三角形）
	throwAimIndicator_ = new ThrowAimIndicator();
	throwAimIndicator_->Initialize(&camera_);

	// 自機はベースモデルを使わず、頭・左腕・右腕のパーツだけで構成する。
	// ※変形後のクローンも同じパーツを使う（GenerateBlocks内のSetClonePartModelsで設定している）。
	// 　クローン専用パーツができたら、そちらだけ差し替えれば自機とは別の見た目にできる。
	player_->SetExtraPartModels({modelPlayerHead_, modelPlayerLeftArm_, modelPlayerRightArm_});

	// クローンの素を持っている間だけ、腕を上げた形のパーツ＋抱えているクローンに差し替える。
	// ※当たり判定のサイズは持っていても変わらない。見た目だけがこちらに切り替わる。
	player_->SetHoldingPartModels(
	    {modelPlayerHead_, modelPlayerLeftArmHolding_, modelPlayerRightArmHolding_, modelPlayerHoldingClone_});

	if (useTitleMap_) {
		// 通常ゲームのUpdateを呼ばないタイトル背景でも、CSVの初期座標を描画行列へ反映する。
		player_->SetModelScaleImmediate(1.5f);
	}
}

void GameScene::Update() {
	// ステージCSVの再読み込みパネル。ポーズ中でも受け付けたいので先頭で呼ぶ。
	ShowStageDebugImGui();

	// ESCでポーズを切り替える。停止中は以降のゲーム処理を更新しない。
	if (Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
		if (isPaused_ && isSettingsOpen_) {
			isSettingsOpen_ = false;
		} else {
			isPaused_ = !isPaused_;
			if (isPaused_) {
				selectedPauseItem_ = 0;
				pauseSelectionAnimationTime_ = 0.0f;
			}
		}
	}
	if (isPaused_) {
		if (isSettingsOpen_) {
			settingsSelectionAnimationTime_ += 1.0f / 60.0f;
			if (Input::GetInstance()->TriggerKey(DIK_W)) {
				selectedSettingsItem_ = (selectedSettingsItem_ + 3) % 4;
				settingsSelectionAnimationTime_ = 0.0f;
				Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
			}
			if (Input::GetInstance()->TriggerKey(DIK_S)) {
				selectedSettingsItem_ = (selectedSettingsItem_ + 1) % 4;
				settingsSelectionAnimationTime_ = 0.0f;
				Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
			}
			const int volumeChange = Input::GetInstance()->TriggerKey(DIK_A)
			                             ? -1
			                             : (Input::GetInstance()->TriggerKey(DIK_D) ? 1 : 0);
			if (volumeChange != 0 && selectedSettingsItem_ == 0) {
				AudioSettings::ChangeSeLevel(volumeChange);
				Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
			} else if (volumeChange != 0 && selectedSettingsItem_ == 1) {
				AudioSettings::ChangeBgmLevel(volumeChange);
				Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
			}

			const float seBarWidth = 128.0f * AudioSettings::GetSeScale();
			const float bgmBarWidth = 128.0f * AudioSettings::GetBgmScale();
			settingsSeVolumeBarSprite_->SetSize({seBarWidth, 16.0f});
			settingsSeVolumeBarSprite_->SetTextureRect({0.0f, 0.0f}, {seBarWidth, 16.0f});
			settingsBgmVolumeBarSprite_->SetSize({bgmBarWidth, 16.0f});
			settingsBgmVolumeBarSprite_->SetTextureRect({0.0f, 0.0f}, {bgmBarWidth, 16.0f});

			constexpr float kSettingsAnimationScale = 0.08f;
			constexpr float kSettingsAnimationSpeed = 6.0f;
			const float settingsPulse =
			    (std::sin(settingsSelectionAnimationTime_ * kSettingsAnimationSpeed) + 1.0f) * 0.5f;
			const float selectedScale = 1.0f + settingsPulse * kSettingsAnimationScale;
			auto setSettingsLayout = [selectedScale](Sprite* sprite, Vector2 position, Vector2 size, bool selected) {
				const float scale = selected ? selectedScale : 1.0f;
				const Vector2 scaledSize = {size.x * scale, size.y * scale};
				sprite->SetSize(scaledSize);
				sprite->SetPosition(
				    {position.x - (scaledSize.x - size.x) * 0.5f, position.y - (scaledSize.y - size.y) * 0.5f});
			};
			setSettingsLayout(settingsSeSprite_, {448.0f, 200.0f}, {64.0f, 64.0f}, selectedSettingsItem_ == 0);
			setSettingsLayout(settingsBgmSprite_, {448.0f, 280.0f}, {64.0f, 64.0f}, selectedSettingsItem_ == 1);
			setSettingsLayout(
			    settingsPredictionSprite_, {448.0f, 360.0f}, {64.0f, 64.0f}, selectedSettingsItem_ == 2);
			setSettingsLayout(
			    settingsPredictionOnSprite_, {656.0f, 376.0f}, {32.0f, 32.0f}, selectedSettingsItem_ == 2);
			setSettingsLayout(
			    settingsPredictionOffSprite_, {656.0f, 376.0f}, {32.0f, 32.0f}, selectedSettingsItem_ == 2);
			setSettingsLayout(settingsCloseSprite_, {384.0f, 440.0f}, {512.0f, 64.0f}, selectedSettingsItem_ == 3);

			const bool changePrediction = selectedSettingsItem_ == 2 &&
			    (Input::GetInstance()->TriggerKey(DIK_A) || Input::GetInstance()->TriggerKey(DIK_D) ||
			     Input::GetInstance()->TriggerKey(DIK_SPACE));
			if (changePrediction) {
				line3D_->SetPredictionVisible(!line3D_->IsPredictionVisible());
				Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
			}
			if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
				if (selectedSettingsItem_ == 3) {
					Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
					isSettingsOpen_ = false;
				}
			}
			return;
		}

		pauseSelectionAnimationTime_ += 1.0f / 60.0f;
		if (Input::GetInstance()->TriggerKey(DIK_W)) {
			selectedPauseItem_ = (selectedPauseItem_ + 3) % 4;
			pauseSelectionAnimationTime_ = 0.0f;
			Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
		}
		if (Input::GetInstance()->TriggerKey(DIK_S)) {
			selectedPauseItem_ = (selectedPauseItem_ + 1) % 4;
			pauseSelectionAnimationTime_ = 0.0f;
			Audio::GetInstance()->PlayWave(cursorMoveSoundHandle_, false, AudioSettings::GetSeVolume());
		}

		constexpr float kMenuWidth = 256.0f;
		constexpr float kMenuHeight = 64.0f;
		constexpr float kAnimationScale = 0.08f;
		constexpr float kAnimationSpeed = 6.0f;
		const float pulse = (std::sin(pauseSelectionAnimationTime_ * kAnimationSpeed) + 1.0f) * 0.5f;
		for (int i = 0; i < static_cast<int>(pauseMenuSprites_.size()); ++i) {
			float width = kMenuWidth;
			float height = kMenuHeight;
			if (i == selectedPauseItem_) {
				width *= 1.0f + pulse * kAnimationScale;
				height *= 1.0f + pulse * kAnimationScale;
			}
			pauseMenuSprites_[i]->SetSize({width, height});
			pauseMenuSprites_[i]->SetPosition(
			    {512.0f - (width - kMenuWidth) * 0.5f, 240.0f + 80.0f * i - (height - kMenuHeight) * 0.5f});
		}

		if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			Audio::GetInstance()->PlayWave(decideSoundHandle_, false, AudioSettings::GetSeVolume());
			switch (selectedPauseItem_) {
			case 0: // もどる
				isPaused_ = false;
				break;
			case 1: // リスタート
				reloadRequested_ = true;
				break;
			case 2: // ステージセレクト
				stageSelectRequested_ = true;
				break;
			case 3: // 設定
				isSettingsOpen_ = true;
				selectedSettingsItem_ = 0;
				settingsSelectionAnimationTime_ = 0.0f;
				break;
			}
		}
		return;
	}

	// マウスカーソルの更新（投げる方向の計算、表示に使用）
	if (mouseCursor_ != nullptr) {
		mouseCursor_->Update();
	}

	// クローンの素を持っている間だけ、投げる方向を示すUI（円＋三角形）を更新する
	if (isHoldingCloneBase_ && throwAimIndicator_ != nullptr) {
		throwAimIndicator_->Update(player_->GetWorldTransform().translation_, mouseCursor_->GetWorldPosition());
	}
	background_->ShowImGui();
	// ポーズ中はここへ来ないので、雲の移動と生成も停止する。
	background_->Update();
	// 変形アニメーションの再生中かどうか（1体でも再生中なら、自機もクローンも操作を受け付けない）
	// ※移動・ジャンプだけでなく、リンクを切る／新しく線を撃つ／素を拾うもすべて止める。
	// 　変形の途中でリンクを切ると、素にもクローンにもなれないまま取り残されてしまうため。
	bool isAnyCloneAnimating = false;
	for (const CloneBase* cloneBase : cloneBases_) {
		if (cloneBase->IsAnimating()) {
			isAnyCloneAnimating = true;
			break;
		}
	}

	if (controlledClone_ && !isAnyCloneAnimating && Input::GetInstance()->IsTriggerMouse(1)) {
		// 自機とのリンクを切ったら、クローンの素（球体）に戻す
		// （現在位置を引き継ぎ、空中なら重力で落下を再開する）
		controlledClone_->ResetToBase();
		controlledClone_ = nullptr;
		cameraController_->SetTarget(player_);
		Audio::GetInstance()->PlayWave(controlSwitchSoundHandle_, false, AudioSettings::GetSeVolume());
	}

	// 現在操作しているキャラクター（通常は自機、クローンを操作中はそのクローンの中のPlayer）
	Player* activePlayer = controlledClone_ ? controlledClone_->GetPlayer() : player_;

	// 帯電中の操作クローンから電気弾を発射（Fキー）
	// ※変形アニメーション中は撃てない
	if (controlledClone_ != nullptr && controlledClone_->IsCharged() && !controlledClone_->IsAnimating() && Input::GetInstance()->TriggerKey(DIK_F)) {

		FireElectricBullet();
	}

	UpdatePressurePlates();
	UpdateDoors();
	UpdateLazers();
	UpdateElectricPlatforms();
	for (ChargePoint* chargePoint : chargePoints_) {
		chargePoint->Update();
	}

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

	// 未取得の鍵は、自機・クローンのどちらも通り抜けられない障害物にする。
	std::vector<MapChipField::Rect> keyObstacleRects;
	for (const Key* key : keys_) {
		if (key->IsCollected()) {
			continue;
		}
		const MapChipField::Rect keyRect = key->GetRect();
		keyObstacleRects.push_back(keyRect);
		cloneBaseRects.push_back(keyRect);
	}

		// エネルギー源を障害物にする
	std::vector<MapChipField::Rect> chargePointRects;
	for (const ChargePoint* chargePoint : chargePoints_) {
		const MapChipField::Rect rect = chargePoint->GetRect();
		chargePointRects.push_back(rect);
		cloneBaseRects.push_back(rect);
	}

	// 電動足場は停止中・移動中に関係なく、プレイヤーとクローンが乗れる障害物にする。
	for (const ElectricPlatform* platform : electricPlatforms_) {
		cloneBaseRects.push_back(platform->GetRect());
	}

	// 感圧板は横から通れる一方通行床として、通常障害物とは分けて扱う。
	std::vector<MapChipField::Rect> oneWayPlatformRects;
	for (const PushPlate* plate : pressurePlates_) {
		oneWayPlatformRects.push_back(plate->GetRect());
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

	// クローンの素を持っているかどうかを伝える（見た目のモデルだけが切り替わる。当たり判定は変わらない）
	player_->SetIsHolding(isHoldingCloneBase_);

	// プレイヤーの更新
	// クローンの素を持っている間、変形アニメーション中はリンク線を発射できないようにする
	bool isTryingToFire = player_->IsOnGround() && !line3D_->IsActive() && !isHoldingCloneBase_ && !isAnyCloneAnimating && Input::GetInstance()->IsTriggerMouse(0);
	bool canActivePlayerMove = !line3D_->IsActive() && !isTryingToFire && !isAnyCloneAnimating && !isGoalCameraCinematic_;
	const bool wasPlayerInWater = player_->IsInWater();
	player_->Update(controlledClone_ == nullptr && canActivePlayerMove, playerObstacleRects, oneWayPlatformRects);
	if (player_->DidJumpThisFrame()) {
		Audio::GetInstance()->PlayWave(jumpSoundHandle_, false, AudioSettings::GetSeVolume());
	}
	if (wasPlayerInWater != player_->IsInWater()) {
		Audio::GetInstance()->PlayWave(waterSplashSoundHandle_, false, AudioSettings::GetSeVolume());
	}

	// レーザーに触れた通常プレイヤーを、レーザーの外側へノックバックさせる。
	if (!player_->IsKnockbackActive()) {
		const Vector3 playerPosition = player_->GetWorldTransform().translation_;
		const MapChipField::Rect playerRectAfterUpdate = {
		    playerPosition.x - player_->GetLeftHalfWidth(), playerPosition.x + player_->GetRightHalfWidth(),
		    playerPosition.y - player_->GetHeight() / 2.0f, playerPosition.y + player_->GetHeight() / 2.0f};
		constexpr float kLazerContactMargin = 0.03f;
		constexpr float kLazerKnockbackHorizontal = 0.12f;
		constexpr float kLazerKnockbackUpward = 0.14f;

		for (const MapChipField::Rect& lazerRect : activeLazerRects) {
			if (!IsRectColliding(playerRectAfterUpdate, ExpandRect(lazerRect, kLazerContactMargin))) {
				continue;
			}

			const float lazerCenterX = (lazerRect.left + lazerRect.right) * 0.5f;
			const float lazerWidth = lazerRect.right - lazerRect.left;
			const float lazerHeight = lazerRect.top - lazerRect.bottom;
			Vector3 knockback{};

			if (lazerHeight > lazerWidth) {
				// 縦レーザーは少し浮かせながら左右へ短く弾く。
				knockback.x = playerPosition.x < lazerCenterX ? -kLazerKnockbackHorizontal : kLazerKnockbackHorizontal;
				knockback.y = kLazerKnockbackUpward;
			} else {
				// 横レーザーも下へ叩き落とさず、短く上へ浮かせる。
				knockback.y = kLazerKnockbackUpward;
			}

			player_->ApplyKnockback(knockback);
			break;
		}
	}

	// 鍵を取得できるのは自機だけ。操作中のクローンは触れても取得しない。
	UpdateKeys(player_);
	UpdateDoors();
	// ゴール判定は自機だけが対象（クローンが扉に触れてもクリアにはしない）
	CheckDoorGoal(player_);

	// レーザーの更新
	for (Lazer* lazer : lazers_) {
		lazer->Update();
	}

	// 電気弾の更新と削除
	for (auto it = electricBullets_.begin(); it != electricBullets_.end();) {

		ElectricBullet* bullet = *it;
		bullet->Update(mapChipField_);

		// ブロックですでに消滅していなければ
		if (!bullet->IsDead()) {
			const MapChipField::Rect bulletRect = bullet->GetRect();

			// 電動足場へ当たった電気弾を消費し、足場の帯電時間を最大まで戻す。
			for (ElectricPlatform* platform : electricPlatforms_) {
				if (IsRectColliding(bulletRect, platform->GetRect())) {
					platform->Charge();
					bullet->SetDead();
					break;
				}
			}

			for (CloneBase* cloneBase : cloneBases_) {
				if (bullet->IsDead()) {
					break;
				}
				// 発射した操作中クローン、変形アニメーション中のものには当てない
				if (cloneBase == controlledClone_ || cloneBase->IsAnimating()) {
					continue;
				}

				if (!IsRectColliding(bulletRect, cloneBase->GetRect())) {
					continue;
				}

				// 当たった時点で弾は必ず消える（帯電できない相手でも弾は消費される）
				bullet->SetDead();

				// 帯電できる相手にだけ電気を渡す

				ChargeClone(cloneBase);

				break;
			}
		}

		if (bullet->IsDead()) {
			delete bullet;
			it = electricBullets_.erase(it);
		} else {
			++it;
		}
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
	// 変形アニメーション中は拾う・投げるの入力も受け付けない
	if (!isAnyCloneAnimating) {
		UpdateCloneBasePickup();
	}

	// 自機の当たり判定矩形（投げたクローンの素が自機の上に乗れるようにするため）
	MapChipField::Rect playerRect;
	playerRect.left = player_->GetWorldTransform().translation_.x - player_->GetLeftHalfWidth();
	playerRect.right = player_->GetWorldTransform().translation_.x + player_->GetRightHalfWidth();
	playerRect.bottom = player_->GetWorldTransform().translation_.y - player_->GetHeight() / 2.0f;
	playerRect.top = player_->GetWorldTransform().translation_.y + player_->GetHeight() / 2.0f;

	// クローンの素の更新（複数配置に対応）
	for (CloneBase* cloneBase : cloneBases_) {
		// この素専用の障害物一覧：閉じているドア、自機、そして「自分以外」のクローン・クローンの素。
		// ※以前は共通のcloneBaseRects（自分自身の矩形も入っている）をそのまま使っていたため、
		// 　自分と自分がぶつかることになり、素同士の当たり判定が成立していなかった。
		// 　ここで自分だけを除いて組み直すことで、素同士・素とクローン・素と自機のすべてが有効になる。
		std::vector<MapChipField::Rect> obstacleRectsForClone = closedDoorRects;
		obstacleRectsForClone.insert(obstacleRectsForClone.end(), keyObstacleRects.begin(), keyObstacleRects.end());
		obstacleRectsForClone.insert(obstacleRectsForClone.end(), chargePointRects.begin(), chargePointRects.end());
		obstacleRectsForClone.push_back(playerRect);
		for (CloneBase* other : cloneBases_) {
			if (other == cloneBase || other == heldCloneBase_) {
				continue;
			}

			obstacleRectsForClone.push_back(other->GetRect());
		}

		// 動く足場を障害物へ追加
		for (const ElectricPlatform* platform : electricPlatforms_) {
			obstacleRectsForClone.push_back(platform->GetRect());
		}

		// 必ず足場を追加した後にUpdateする
		cloneBase->Update(cloneBase == controlledClone_ && canActivePlayerMove, obstacleRectsForClone, playerRect, oneWayPlatformRects);
		if (cloneBase->ConsumeThrownLanding()) {
			Audio::GetInstance()->PlayWave(cloneLandingSoundHandle_, false, AudioSettings::GetSeVolume());
		}

		if (cloneBase->ConsumeWaterDestroyed()) {
			// 持っている素がプレイヤーと一緒に水へ入った場合、CloneBase側だけでなく
			// GameSceneとプレイヤー側の所持状態も同時に解除する。
			// ここを残したままだと、素は初期位置へ戻っているのに持つ見た目や
			// 投げる処理だけが継続してしまう。
			if (heldCloneBase_ == cloneBase) {
				heldCloneBase_ = nullptr;
				isHoldingCloneBase_ = false;
				player_->SetIsHolding(false);
			}

			// 消滅したクローンを操作していた場合
			if (controlledClone_ == cloneBase) {
				controlledClone_ = nullptr;
				Audio::GetInstance()->PlayWave(controlSwitchSoundHandle_, false, AudioSettings::GetSeVolume());

				// 接続線を切る
				line3D_->ResetLine();

				// カメラを通常プレイヤーへ戻す
				cameraController_->SetTarget(player_);
			}
		}
	}

	UpdateChargeSources();
	UpdateChargeTransfer();
	ResetChargeHistoryIfAllUsed();

	// 取りこぼし対策：誰にも操作されていないのにクローンのまま残っているものは、素に戻す。
	// ※通常はリンクを切った時点で戻るが、何らかの理由で操作権だけ外れた場合に
	// 　素にもクローンにもなれないまま取り残されるのを防ぐ
	for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase != controlledClone_ && cloneBase->GetState() == CloneBase::State::kTransformed) {
			cloneBase->ResetToBase();
		}
	}

	// 帯電の受け渡し（接触）と、全員使い切った場合の周回リセット
	UpdateChargeTransfer();
	ResetChargeHistoryIfAllUsed();

	// ImGui上でクローンの素の配置・状態を管理するパネルを表示する
	ShowCloneBaseManagerImGui();

	// ImGui上でこのステージのカメラ設定を調整するパネルを表示する
	ShowCameraImGui();

	// 天球の更新
	skydome_->Update();

	// 鍵取得演出中だけ通常の追従を止め、ゴール紹介用のカメラを動かす。
	if (isGoalCameraCinematic_) {
		UpdateGoalCameraCinematic();
	} else {
		cameraController_->Update();
	}

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

	// プレイヤー操作中かつ、クローンの素を持っていない間だけリンク線を発射できる
	bool canFireLine = controlledClone_ == nullptr && activePlayer->IsOnGround() && !isHoldingCloneBase_ && !isAnyCloneAnimating && !isGoalCameraCinematic_;
	line3D_->Update(
	    activePlayer->GetWorldTransform().translation_, camera_, mapChipField_, closedDoorRects, activeLazerRects,
	    canFireLine, controlledClone_ != nullptr);

	// 今どのクローンにつながっているかを示す線を更新する（自機の中心と、クローンの中心を結ぶ）
	// 濃さは変形アニメーションに合わせる。
	// 　素→クローン：アニメーションの間ずっとフェードインし、クローンの姿になった瞬間に完全表示になる
	// 　クローン→素：アニメーションの間ずっとフェードアウトし、素に戻った瞬間に消えている
	if (controlledClone_ != nullptr) {
		float alpha = controlledClone_->IsAnimating() ? controlledClone_->GetAnimationProgress() : 1.0f;
		line3D_->SetConnectedLine(player_->GetWorldTransform().translation_, controlledClone_->GetWorldTransform().translation_, alpha);
	} else {
		// リンクを切った直後は操作権がもう自機に戻っているので、
		// 素に戻るアニメーションを再生中のクローンを探して、そこへ向けてフェードアウトさせる
		CloneBase* revertingClone = nullptr;
		for (CloneBase* cloneBase : cloneBases_) {
			if (cloneBase->GetState() == CloneBase::State::kReverting) {
				revertingClone = cloneBase;
				break;
			}
		}

		if (revertingClone != nullptr) {
			line3D_->SetConnectedLine(
			    player_->GetWorldTransform().translation_, revertingClone->GetWorldTransform().translation_,
			    1.0f - revertingClone->GetAnimationProgress());
		} else {
			line3D_->ClearConnectedLine();
		}
	}

	if (controlledClone_ == nullptr && line3D_->IsActive() && !line3D_->IsCloneLine()) {

		for (CloneBase* cloneBase : cloneBases_) {
			// 変形アニメーション中のものにはつながらない
			if (cloneBase->GetState() != CloneBase::State::kBase) {
				continue;
			}

			if (line3D_->IsTouchingSphere(cloneBase->GetWorldTransform().translation_, CloneBase::kCollisionRadius)) {

				cloneBase->Transform();
				controlledClone_ = cloneBase;
				Audio::GetInstance()->PlayWave(controlSwitchSoundHandle_, false, AudioSettings::GetSeVolume());
				// ※カメラはクローンに追従しない（自機基準のまま固定／横スクロール）

				// クローンに当たった線を消す
				line3D_->ResetLine();

				break;
			}
		}
	}
}

void GameScene::UpdateTitleBackground() {
	if (background_ != nullptr) {
		background_->Update();
	}
}

void GameScene::Draw() { DrawWorld(true); }

void GameScene::DrawTitleBackground() { DrawWorld(false); }

void GameScene::DrawWorld(bool drawGameplayUi) {
	// 天球と透過画像の板は最背面。深度を書かず、後から描く雲やゲーム本体を隠さない。
	Model::PreDraw(Model::CullingMode::kBack, Model::BlendMode::kNormal, Model::DepthTestMode::kOff);
	skydome_->Draw();
	background_->DrawBackground();
	Model::PostDraw();

	// 雲同士や雲の表裏には通常の3D深度判定を使う。
	Model::PreDraw();
	background_->DrawClouds();
	Model::PostDraw();
	// 背景演出の深度をここで区切り、カメラを遠ざけても雲が自機を隠さないようにする。
	DirectXCommon::GetInstance()->ClearDepthBuffer();
	Model::PreDraw();

	// クローンの素の描画（球体、または線接続後は自機と同じ形）
	// 持たれている間は表示しない（代わりに自機側が「持っている状態」の見た目を担当する）
	for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase->IsHeld()) {
			continue;
		}
		cloneBase->Draw();
	}

	// プレイヤーの描画
	player_->Draw();

	// リンク線の描画（板ポリゴンなので3Dモデルの描画パスの中で描く）
	// 「飛ばす接続リンク線」と「今どれにつながっているかを示すリンク線」の両方をここでまとめて描画する
	line3D_->Draw(camera_);

	// 投げる方向を示す三角形も板ポリゴンなので、ここで描く
	if (isHoldingCloneBase_ && throwAimIndicator_ != nullptr) {
		throwAimIndicator_->DrawModel();
	}

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

	for (ChargePoint* chargePoint : chargePoints_) {
		chargePoint->Draw();
	}

	for (ElectricPlatform* platform : electricPlatforms_) {
		platform->Draw();
	}

	// 水の描画
	for (auto& line : worldTransformWaters_) {
		for (WorldTransform* water : line) {
			if (water) {
				modelWater_->Draw(*water, camera_);
			}
		}
	}

	// 電気弾の描画
	for (ElectricBullet* bullet : electricBullets_) {
		bullet->Draw();
	}

	Model::PostDraw();

	if (!drawGameplayUi) {
		return;
	}

	// 3Dモデルより手前へマウスカーソルの円を描画する
	if (mouseCursor_ != nullptr) {
		mouseCursor_->Draw();
	}

	// クローンの素を持っている間だけ、投げる方向を示す円（ワイヤーフレーム）を描画する
	// ※三角形は板ポリゴンなので、3Dモデルの描画パスの中ですでに描いている
	if (isHoldingCloneBase_ && throwAimIndicator_ != nullptr) {
		throwAimIndicator_->Draw();
	}

	// 自機の当たり判定サイズを可視化するワイヤーフレーム（Debugビルド/USE_IMGUIの時だけ表示）
	DrawCollisionWireframes();

	// 常時表示する小さなポーズ操作案内
	Sprite::PreDraw();
	if (isPaused_) {
		pauseOverlaySprite_->Draw();
		if (isSettingsOpen_) {
			settingsSeSprite_->Draw();
			if (AudioSettings::seLevel > 0) {
				settingsSeVolumeBarSprite_->Draw();
			}
			settingsBgmSprite_->Draw();
			if (AudioSettings::bgmLevel > 0) {
				settingsBgmVolumeBarSprite_->Draw();
			}
			settingsPredictionSprite_->Draw();
			if (line3D_->IsPredictionVisible()) {
				settingsPredictionOnSprite_->Draw();
			} else {
				settingsPredictionOffSprite_->Draw();
			}
			settingsCloseSprite_->Draw();
		} else {
			for (Sprite* sprite : pauseMenuSprites_) {
				sprite->Draw();
			}
		}
	}
	pauseEscSprite_->Draw();
	pausePoseGuideSprite_->Draw();
	Sprite::PostDraw();
}

///// ----- 自機の当たり判定サイズを可視化するワイヤーフレーム ----- /////
// ImGuiパネルと同じ扱い（USE_IMGUIが定義されるDebugビルドの時だけ表示される。Releaseでは何もしない）
void GameScene::DrawCollisionWireframes() {
#ifdef USE_IMGUI
	/// --- 自機 ---
	const Vector3& playerPosition = player_->GetWorldTransform().translation_;
	// 持っている間は左右非対称（向いている方向側だけが伸びる）になるので、左右別々に半幅を取る
	float playerHalfHeight = player_->GetHeight() / 2.0f;

	MapChipField::Rect playerRect;
	playerRect.left = playerPosition.x - player_->GetLeftHalfWidth();
	playerRect.right = playerPosition.x + player_->GetRightHalfWidth();
	playerRect.bottom = playerPosition.y - playerHalfHeight;
	playerRect.top = playerPosition.y + playerHalfHeight;

	// 持っている間は色を変えて、今どちらの状態かひと目でわかるようにする
	Vector4 playerColor = player_->IsHolding() ? Vector4{1.0f, 0.5f, 0.0f, 1.0f} : Vector4{0.0f, 1.0f, 1.0f, 1.0f};
	DrawRectWireframe(playerRect, playerPosition.z, playerHalfHeight, playerColor);

	/// --- クローンの素・クローン ---
	// GetRect()は当たり判定でそのまま使っている矩形なので、めり込みやガタつきの原因を目で追える
	for (const CloneBase* cloneBase : cloneBases_) {
		// 持たれている間は画面に出していないので、枠も出さない
		if (cloneBase->IsHeld()) {
			continue;
		}

		const MapChipField::Rect rect = cloneBase->GetRect();
		const Vector3& position = cloneBase->GetWorldTransform().translation_;

		// 変形後のクローンは赤紫、素の球体は黄緑
		Vector4 color = (cloneBase->GetState() == CloneBase::State::kTransformed) ? Vector4{1.0f, 0.2f, 1.0f, 1.0f}
		                                                                         : Vector4{0.6f, 1.0f, 0.2f, 1.0f};

		DrawRectWireframe(rect, position.z, (rect.top - rect.bottom) / 2.0f, color);
	}
#endif
}

///// ----- 矩形1つぶんのワイヤーフレームの箱 ----- /////
// 当たり判定はX・Yの軸に沿った矩形（AABB）なので、奥行き(Z)と回転は判定に使っていない。
// 箱の奥行きは、見た目を立方体に近づけるための飾り。
void GameScene::DrawRectWireframe(const MapChipField::Rect& rect, float centerZ, float halfDepth, const Vector4& color) {
	Vector3 corners[8] = {
	    {rect.left,  rect.bottom, centerZ - halfDepth},
        {rect.right, rect.bottom, centerZ - halfDepth},
	    {rect.right, rect.top,    centerZ - halfDepth},
        {rect.left,  rect.top,    centerZ - halfDepth},
	    {rect.left,  rect.bottom, centerZ + halfDepth},
        {rect.right, rect.bottom, centerZ + halfDepth},
	    {rect.right, rect.top,    centerZ + halfDepth},
        {rect.left,  rect.top,    centerZ + halfDepth},
	};

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
}

///// ----- カメラ設定パネル ----- /////
// NOTE: ImGuiの表示文字列は日本語だと文字化けするため、英語表記にしている
void GameScene::ShowCameraImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Camera");

	ImGui::Text("Stage: %d", stageNumber_);
	ImGui::TextWrapped("The camera does not follow the player. It stays where you set it here.");
	ImGui::Separator();

	CameraController::StageSetting& setting = cameraController_->GetCurrentSettingRef();

	ImGui::DragFloat3("Position", &setting.position.x, 0.1f);
	ImGui::DragFloat3("Rotation (deg)", &setting.rotationDegree.x, 1.0f);
	ImGui::Separator();

	// 横スクロール（自機のX座標に合わせてカメラも横に動く）
	ImGui::Checkbox("Scroll X", &setting.isScrollX);
	ImGui::DragFloat("Scroll Offset X", &setting.scrollOffsetX, 0.1f);
	ImGui::DragFloat("Scroll Min X", &setting.scrollMinX, 0.1f);
	ImGui::DragFloat("Scroll Max X", &setting.scrollMaxX, 0.1f);
	ImGui::Separator();

	// 縦スクロール（自機のY座標に合わせてカメラも縦に動く）
	ImGui::Checkbox("Scroll Y", &setting.isScrollY);
	ImGui::DragFloat("Scroll Offset Y", &setting.scrollOffsetY, 0.1f);
	ImGui::DragFloat("Scroll Min Y", &setting.scrollMinY, 0.1f);
	ImGui::DragFloat("Scroll Max Y", &setting.scrollMaxY, 0.1f);
	ImGui::TextWrapped("Min >= Max means no limit. Position of a scrolling axis is ignored.");
	ImGui::Separator();

	ImGui::Text(
	    "Current: (%.2f, %.2f, %.2f)", camera_.translation_.x, camera_.translation_.y, camera_.translation_.z);
	ImGui::Separator();

	// 調整した内容をCSVへ保存する（次回起動時はこの値で始まる）
	if (ImGui::Button("Save To CSV")) {
		cameraSaveMessage_ =
		    cameraController_->SaveStageSettingsCsv(kCameraSettingsCsvPath) ? "Saved" : "Save failed";
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload From CSV")) {
		cameraController_->LoadStageSettingsCsv(kCameraSettingsCsvPath);
		cameraController_->SelectStage(stageNumber_);
		cameraController_->Reset();
		cameraSaveMessage_ = "Reloaded";
	}
	ImGui::Text("%s", cameraSaveMessage_);

	ImGui::End();
#endif
}

///// ----- ステージ再読み込みパネル ----- /////
// NOTE: ImGuiの表示文字列は日本語だと文字化けするため、英語表記にしている
void GameScene::ShowStageDebugImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Stage Debug");

	/// --- 今読み込んでいるステージの情報 ---
	ImGui::Text("Stage: %d", stageNumber_);
	ImGui::Text("Map File: Resources/map/map_%d.csv", stageNumber_);
	ImGui::TextWrapped("Edit the csv in a text editor and save it, then press Reload to rebuild the stage.");
	ImGui::Separator();

	/// --- 再読み込み ---
	if (ImGui::Button("Reload Stage (F5)")) {
		// SceneManagerがこの要求を見て、ゲームシーンを作り直す
		reloadRequested_ = true;
	}
	ImGui::TextWrapped("The player returns to the start position. Camera and background settings are reloaded too.");

	ImGui::End();
#endif

	// ImGuiのウィンドウを閉じていても押せるように、キーでも再読み込みできるようにする
	if (Input::GetInstance()->TriggerKey(DIK_F5)) {
		reloadRequested_ = true;
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
				// 自機はベースモデル（立方体）を持たず、頭・左腕・右腕のパーツだけで構成するので
				// ベースモデルにはnullptrを渡す（パーツはInitialize()の最後でまとめて設定している）
				player_->Initialize(nullptr, &camera_, playerPosition);
				player_->SetMapChipField(mapChipField_);
				// モデルの見た目を少し大きく表示する（当たり判定サイズは変わらない。ImGuiで調整可能）
				player_->SetModelScale(1.5f);
				break;
			}
			case MapChipType::kLazer: {
				uint8_t subID = mapChipField_->GetMapChipSubIDByIndex(j, i);
				lazerPositionsByID[subID].push_back(mapChipField_->GetMapChipPositionByIndex(j, i));
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
			case MapChipType::kWater: {
				// 水はこのループの後で、隣接するマスを一つの直方体にまとめて生成する。
				worldTransformWaters_[i][j] = nullptr;
				break;
			}
			case MapChipType::kCloneBase: {
				// クローンの素を生成（CSV上の "C0"。複数配置に対応）
				Vector3 cloneBasePosition = mapChipField_->GetMapChipPositionByIndex(j, i);
				CloneBase* cloneBase = new CloneBase();
				// 変形後のクローンは自機と同じくベースモデルを持たず、パーツだけで構成するのでnullptrを渡す。
				// クローン専用の1体モデルを用意したら、ここへ渡すだけでそちらが使われる。
				cloneBase->Initialize(modelCloneBase_, nullptr, &camera_, mapChipField_, cloneBasePosition);
				// いったんは自機と同じ頭・左腕・右腕を使う。
				// クローン専用パーツができたら、ここへ渡すモデルを差し替えるだけで見た目が切り替わる。
				cloneBase->SetClonePartModels({modelPlayerHead_, modelPlayerLeftArm_, modelPlayerRightArm_});
				// 見た目の大きさも自機と揃える（当たり判定サイズには影響しない）
				cloneBase->SetCloneModelScale(1.5f);
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
				plate->Initialize(modelPushPlateBase_, modelPushPlateButton_, &camera_, centerPosition, plateID, requiredCount, plateWidth);
				pressurePlates_.push_back(plate);
				for (uint32_t plateX = j; plateX <= endX; ++plateX) {
					worldTransformBlocks_[i][plateX] = nullptr;
				}
				break;
			}
			case MapChipType::kDoor: {
				Door* door = new Door();
				door->Initialize(
				    modelDoor_, modelDoorOpen_, modelDoorOpenGlass_, &camera_, mapChipField_->GetMapChipPositionByIndex(j, i), mapChipField_->GetMapChipSubIDByIndex(j, i));
				doors_.push_back(door);
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
			case MapChipType::kKey: {
				Key* key = new Key();

				key->Initialize(modelKey_, &camera_, mapChipField_->GetMapChipPositionByIndex(j, i), mapChipField_->GetMapChipSubIDByIndex(j, i));

				keys_.push_back(key);
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
			case MapChipType::kChargePoint: {
				ChargePoint* chargePoint = new ChargePoint();

				chargePoint->Initialize(modelChargePoint_, &camera_, mapChipField_->GetMapChipPositionByIndex(j, i));

				chargePoints_.push_back(chargePoint);
				worldTransformBlocks_[i][j] = nullptr;
				break;
			}
			case MapChipType::kElectricPlatform: {
				const uint8_t subID = mapChipField_->GetMapChipSubIDByIndex(j, i);
				const char direction = mapChipField_->GetMovementDirectionByIndex(j, i);
				const float distance = static_cast<float>(mapChipField_->GetMovementDistanceByIndex(j, i));
				const Vector3 start = mapChipField_->GetMapChipPositionByIndex(j, i);
				Vector3 end = start;

				switch (direction) {
				case 'L':
					end.x -= distance * MapChipField::kBlockWidth;
					break;
				case 'U':
					end.y += distance * MapChipField::kBlockHeight;
					break;
				case 'D':
					end.y -= distance * MapChipField::kBlockHeight;
					break;
				case 'R':
				default:
					end.x += distance * MapChipField::kBlockWidth;
					break;
				}

				ElectricPlatform* platform = new ElectricPlatform();
				platform->Initialize(modelElectricPlatform_, &camera_, start, end, subID);
				electricPlatforms_.push_back(platform);
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

	// 隣接する水マスを可能な限り大きな長方形へまとめる。
	// 1マスずつ透明な箱を重ねず、水域ごとに一つの直方体を描くことで、
	// 正面・上面・左右端の側面を残しつつ内部の継ぎ目をなくす。
	std::vector<std::vector<bool>> waterUsed(numBlockVertical, std::vector<bool>(numBlockHorizontal, false));
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		for (uint32_t j = 0; j < numBlockHorizontal; ++j) {
			if (waterUsed[i][j] || mapChipField_->GetMapChipTypeByIndex(j, i) != MapChipType::kWater) {
				continue;
			}

			uint32_t endX = j;
			while (endX + 1 < numBlockHorizontal && !waterUsed[i][endX + 1] &&
			       mapChipField_->GetMapChipTypeByIndex(endX + 1, i) == MapChipType::kWater) {
				++endX;
			}

			uint32_t endY = i;
			for (uint32_t nextY = i + 1; nextY < numBlockVertical; ++nextY) {
				bool canExtend = true;
				for (uint32_t x = j; x <= endX; ++x) {
					if (waterUsed[nextY][x] || mapChipField_->GetMapChipTypeByIndex(x, nextY) != MapChipType::kWater) {
						canExtend = false;
						break;
					}
				}
				if (!canExtend) {
					break;
				}
				endY = nextY;
			}

			for (uint32_t y = i; y <= endY; ++y) {
				for (uint32_t x = j; x <= endX; ++x) {
					waterUsed[y][x] = true;
				}
			}

			const float width = static_cast<float>(endX - j + 1) * MapChipField::kBlockWidth;
			const float height = static_cast<float>(endY - i + 1) * MapChipField::kBlockHeight;
			const Vector3 topLeft = mapChipField_->GetMapChipPositionByIndex(j, i);
			const Vector3 bottomRight = mapChipField_->GetMapChipPositionByIndex(endX, endY);

			WorldTransform* water = new WorldTransform();
			water->Initialize();
			water->scale_ = {width, height, 1.0f};
			water->translation_ = {
			    (topLeft.x + bottomRight.x) / 2.0f,
			    bottomRight.y - MapChipField::kBlockHeight / 2.0f,
			    topLeft.z,
			};
			UpdateWorldTransform(*water);
			worldTransformWaters_[i][j] = water;
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
	float& chargeDuration = CloneBase::GetChargeDurationSecondsRef();
	ImGui::SliderFloat("Charge Duration", &chargeDuration, 0.5f, 30.0f, "%.1f sec");
	ImGui::Separator();

	// 当たり判定・拾えるかどうかの状態（仮実装）
	ImGui::Text("Colliding With CloneBase: %s", isCollidingWithCloneBase_ ? "True" : "False");
	ImGui::Text("Can Pick Up: %s", canPickUpCloneBase_ ? "True" : "False");
	ImGui::Text("Holding CloneBase: %s", isHoldingCloneBase_ ? "True" : "False");

	// 当たり判定サイズの調整（自機・クローン共通。持っていてもサイズは変わらない）
	ImGui::SliderFloat("Player Hitbox Width", &Player::GetWidthRef(), 0.1f, 0.8f);
	ImGui::SliderFloat("Player Hitbox Height", &Player::GetHeightRef(), 0.1f, 0.8f);
	// クローンの素の当たり判定サイズの調整
	ImGui::SliderFloat("CloneBase Hitbox Width", &CloneBase::GetWidthRef(), 0.1f, 0.8f);
	ImGui::SliderFloat("CloneBase Hitbox Height", &CloneBase::GetHeightRef(), 0.1f, 0.8f);
	// 自機モデルの見た目の大きさを調整する（当たり判定サイズには影響しない）
	ImGui::SliderFloat("Player Model Scale", &player_->GetModelScaleRef(), 0.5f, 3.0f);
	ImGui::Separator();

	// 投げる力を調整する（距離に関わらず常にこの力で投げる）
	ImGui::SliderFloat("Throw Power", &throwPower_, 0.0f, kMaxThrowPower);

	// 投げる方向を示すUI（円＋三角形）の大きさを調整する
	if (throwAimIndicator_ != nullptr) {
		ImGui::SliderFloat("Aim Circle Radius", &throwAimIndicator_->circleRadius_, 0.5f, 5.0f);
		ImGui::SliderFloat("Aim Triangle Size", &throwAimIndicator_->triangleSize_, 0.1f, 2.0f);
	}
	ImGui::Separator();

	if (cloneBases_.empty()) {
		// まだ1体も配置されていない場合
		ImGui::Text("No clone bases placed");
	} else {
		for (size_t i = 0; i < cloneBases_.size(); ++i) {
			CloneBase* cloneBase = cloneBases_[i];
			const Vector3& pos = cloneBase->GetWorldTransform().translation_;

			// 変形アニメーション中かどうかも分かるように表示する
			const char* stateText = "Base (Not Connected)";
			switch (cloneBase->GetState()) {
			case CloneBase::State::kTransforming:
				stateText = "Transforming...";
				break;
			case CloneBase::State::kTransformed:
				stateText = "Transformed (Line Connected)";
				break;
			case CloneBase::State::kReverting:
				stateText = "Reverting...";
				break;
			default:
				break;
			}

			ImGui::PushID(static_cast<int>(i));
			ImGui::Text("Clone[%zu] Pos:(%.1f, %.1f, %.1f)", i, pos.x, pos.y, pos.z);
			ImGui::Text("State: %s%s", stateText, cloneBase->IsHeld() ? " [Held]" : "");
			ImGui::Text("Charged: %s / Remaining: %.1f sec / Used: %s", cloneBase->IsCharged() ? "YES" : "NO", cloneBase->GetChargeRemainingSeconds(), cloneBase->HasBeenCharged() ? "YES" : "NO");

			// 帯電のデバッグ操作（電気弾の発射や受け渡しを試すための入口）
			if (ImGui::Button("Charge (Debug)")) {
				ChargeClone(cloneBase);
			}
			ImGui::SameLine();
			if (ImGui::Button("Discharge (Debug)")) {
				cloneBase->Discharge();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Used Flag (Debug)")) {
				cloneBase->ResetChargeHistory();
			}

			// リンクのデバッグ操作（操作権とカメラもあわせて切り替える）
			if (cloneBase->GetState() == CloneBase::State::kTransformed) {
				if (ImGui::Button("Reset To Base (Debug)")) {
					cloneBase->ResetToBase();
					if (controlledClone_ == cloneBase) {
						controlledClone_ = nullptr;
						line3D_->ResetLine();
						cameraController_->SetTarget(player_);
					}
				}
			} else if (cloneBase->GetState() == CloneBase::State::kBase) {
				if (ImGui::Button("Connect Line (Debug)") && controlledClone_ == nullptr) {
					cloneBase->Transform();
					controlledClone_ = cloneBase;
				}
			}
			ImGui::PopID();
			ImGui::Separator();
		}
	}

	ImGui::End();
#endif
}

///// ----- クローンの素を持つ処理（仮実装） ----- /////
// プレイヤーとクローンの素（球体）の当たり判定を取り、当たっている間にスペースキーを押すと持つ。
// 今は「触れていたら拾える」実装。将来的には自機を中心とした円の半径内なら拾えるようにする予定。
void GameScene::UpdateCloneBasePickup() {
	isCollidingWithCloneBase_ = false;
	canPickUpCloneBase_ = false;

	const Vector3& playerPos = player_->GetWorldTransform().translation_;
	float playerHalfWidth = player_->GetWidth() / 2.0f;
	float playerHalfHeight = player_->GetHeight() / 2.0f;

	// クローンの素を持っている間は、自機の「持っている状態」の見た目・当たり判定サイズで表現するため、
	// クローンの素自体はもう画面には出さない（Draw側で非表示にしている）。
	// 位置は投げる時の発射位置として使うだけなので、単純に自機の中心に合わせておけば十分。
	if (isHoldingCloneBase_ && heldCloneBase_) {
		// 持っている間にスペースキーを押したら、マウスカーソル方向へ投げる
		if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			ThrowHeldCloneBase();
			return;
		}

		heldCloneBase_->SetTranslation(playerPos);
		return;
	}

	// (以降、当たり判定と「拾う」入力を確認する処理は変更なし)
	for (CloneBase* cloneBase : cloneBases_) {

		// 素の状態のものだけ拾える（クローン、変形アニメーション中のものは対象外）
		if (cloneBase->GetState() != CloneBase::State::kBase) {
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

void GameScene::UpdatePressurePlates() {
	std::vector<Player*> actors = {player_};
	// 変身前のクローンの素（球体）はPlayerではないので、矩形として別に渡す
	std::vector<MapChipField::Rect> cloneBaseRectsForPlate;
	for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase->GetState() == CloneBase::State::kTransformed) {
			actors.push_back(cloneBase->GetPlayer());
			continue;
		}
		// 持たれている間は自機の中に位置しているだけなので、感圧板の判定には含めない
		if (cloneBase->IsHeld()) {
			continue;
		}
		cloneBaseRectsForPlate.push_back(cloneBase->GetRect());
	}
	for (PushPlate* plate : pressurePlates_) {
		const bool wasPushed = plate->IsPushed();
		plate->Update(actors, cloneBaseRectsForPlate);
		if (!wasPushed && plate->IsPushed()) {
			Audio::GetInstance()->PlayWave(pushPlateSoundHandle_, false, AudioSettings::GetSeVolume());
		}
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
		const bool wasCollected = key->IsCollected();
		key->Update(activePlayer);
		if (!wasCollected && key->IsCollected()) {
			Audio::GetInstance()->PlayWave(keyGetSoundHandle_, false, AudioSettings::GetSeVolume());
			StartGoalCameraCinematic(key->GetID());
		}
	}
}

void GameScene::StartGoalCameraCinematic(uint8_t keyID) {
	if (isGoalCameraCinematic_) {
		return;
	}

	const Door* targetDoor = nullptr;
	for (const Door* door : doors_) {
		if (door->GetID() == keyID) {
			targetDoor = door;
			break;
		}
	}
	if (targetDoor == nullptr) {
		return;
	}

	isGoalCameraCinematic_ = true;
	goalCameraPhase_ = GoalCameraPhase::kFocus;
	goalCameraTimer_ = 0.0f;
	goalCameraDoorID_ = keyID;
	goalCameraStart_ = camera_.translation_;

	const MapChipField::Rect doorRect = targetDoor->GetRect();
	goalCameraFocus_ = {
	    (doorRect.left + doorRect.right) * 0.5f,
	    doorRect.bottom + kGoalCameraFloorViewOffsetY,
	    (std::max)(goalCameraStart_.z, -10.0f),
	};
}

void GameScene::UpdateGoalCameraCinematic() {
	goalCameraTimer_ += 1.0f / 60.0f;

	switch (goalCameraPhase_) {
	case GoalCameraPhase::kFocus: {
		const float t = SmoothStep(goalCameraTimer_ / kGoalCameraFocusDuration);
		camera_.translation_ = LerpVector3(goalCameraStart_, goalCameraFocus_, t);
		if (goalCameraTimer_ >= kGoalCameraFocusDuration) {
			goalCameraPhase_ = GoalCameraPhase::kFocusHold;
			goalCameraTimer_ = 0.0f;
		}
		break;
	}
	case GoalCameraPhase::kFocusHold:
		camera_.translation_ = goalCameraFocus_;
		if (goalCameraTimer_ >= kGoalCameraFocusHoldDuration) {
			goalCameraPhase_ = GoalCameraPhase::kReturn;
			goalCameraTimer_ = 0.0f;
		}
		break;
	case GoalCameraPhase::kReturn: {
		const float t = SmoothStep(goalCameraTimer_ / kGoalCameraReturnDuration);
		camera_.translation_ = LerpVector3(goalCameraFocus_, goalCameraStart_, t);
		if (goalCameraTimer_ >= kGoalCameraReturnDuration) {
			camera_.translation_ = goalCameraStart_;
			isGoalCameraCinematic_ = false;
		}
		break;
	}
	}

	camera_.UpdateMatrix();
}

void GameScene::UpdateDoors() {
	for (Door* door : doors_) {
		bool shouldOpen = false;

		for (const Key* key : keys_) {
			if (key->GetID() == door->GetID() && key->IsCollected()) {
				// 今紹介しているドアだけは、ズームが終わるまで開き始めない。
				const bool isWaitingForCamera = isGoalCameraCinematic_ &&
				                                goalCameraPhase_ == GoalCameraPhase::kFocus &&
				                                door->GetID() == goalCameraDoorID_;
				shouldOpen = !isWaitingForCamera;
				break;
			}
		}

		door->SetOpen(shouldOpen);
		door->Update();
	}
}

///// ----- ゴール判定 ----- /////
// 開いている扉に「自機」が触れたらクリア。
// ※クローンは見た目が自機と同じでも、ここには渡さないのでゴールにはならない
void GameScene::CheckDoorGoal(const Player* goalPlayer) {

	for (const Door* door : doors_) {
		if (door->IsOpen() && door->IsCollidingWithPlayer(goalPlayer)) {

			isFinished_ = true;
			return;
		}
	}
}

void GameScene::ChargeClone(CloneBase* cloneBase) {
	const bool wasCharged = cloneBase->IsCharged();
	cloneBase->Charge();
	if (!wasCharged && cloneBase->IsCharged()) {
		Audio::GetInstance()->PlayWave(electricChargeSoundHandle_, false, AudioSettings::GetSeVolume());
	}
}

void GameScene::FireElectricBullet() {
	if (controlledClone_ == nullptr || !controlledClone_->IsCharged() || controlledClone_->IsAnimating()) {
		return;
	}

	Player* clonePlayer = controlledClone_->GetPlayer();

	Vector3 position = clonePlayer->GetWorldTransform().translation_;

	float direction = clonePlayer->GetLRDirection() == Player::LRDirection::kRight ? 1.0f : -1.0f;

	// クローンの正面から出す
	position.x += direction * 0.6f;

	Vector3 velocity = {direction * 0.2f, 0.0f, 0.0f};

	ElectricBullet* bullet = new ElectricBullet();

	bullet->Initialize(modelElectricBullet_, &camera_, position, velocity);

	electricBullets_.push_back(bullet);
	Audio::GetInstance()->PlayWave(electricFireSoundHandle_, false, AudioSettings::GetSeVolume());

	// 発射時に帯電を消費
	controlledClone_->Discharge();
}

///// ----- 帯電の受け渡し（接触） ----- /////
// 帯電しているクローン・素が、帯電していない素に触れると電気が移る。
// 渡した側の電気は消え、受け取った側だけが帯電している状態になる。
void GameScene::UpdateChargeTransfer() {
	// このフレームの開始時点で帯電していたものだけを「渡す側」として確定させる。
	// ※その場で移していくと、受け取った相手が同じフレーム中にさらに次へ渡してしまい、
	// 　1フレームで電気が数珠つなぎに飛んでいってしまう
	std::vector<CloneBase*> givers;
	for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase->IsCharged() && !cloneBase->IsHeld() && !cloneBase->IsAnimating()) {
			givers.push_back(cloneBase);
		}
	}

	for (CloneBase* giver : givers) {
		// 受け渡しの途中で自分の電気が無くなった場合は打ち切る
		if (!giver->IsCharged()) {
			continue;
		}

		// 当たり判定で押し戻されて矩形が重ならないので、あそびを持たせた矩形で接触を見る
		const MapChipField::Rect giverRect = ExpandRect(giver->GetRect(), kChargeContactMargin);

		for (CloneBase* receiver : cloneBases_) {
			if (receiver == giver || receiver->IsHeld() || receiver->IsAnimating()) {
				continue;
			}

			// この周回でまだ一度も帯電していない相手にだけ渡せる
			if (!receiver->CanBeCharged()) {
				continue;
			}

			if (!IsRectColliding(giverRect, receiver->GetRect())) {
				continue;
			}

			// 電気を移す（渡した側からは消える）
			ChargeClone(receiver);
			giver->Discharge();
			break;
		}
	}
}

/// --- 帯電の周回リセット ---
// 全てのクローン・素が帯電を使い終わったら（誰も帯電しておらず、全員が一度は帯電済み）、
// 帯電履歴をまとめて消して、また最初のどれかが帯電できるようにする。
// ※これがないと、ギミックを解くのに失敗した時点で二度と帯電できず詰んでしまう
void GameScene::ResetChargeHistoryIfAllUsed() {
	if (cloneBases_.empty()) {
		return;
	}

	for (const CloneBase* cloneBase : cloneBases_) {
		// まだ帯電中のものがいる、またはまだ一度も帯電していないものがいるならリセットしない
		if (cloneBase->IsCharged() || !cloneBase->HasBeenCharged()) {
			return;
		}
	}

	for (CloneBase* cloneBase : cloneBases_) {
		cloneBase->ResetChargeHistory();
	}
}

void GameScene::UpdateChargeSources() {
	for (CloneBase* cloneBase : cloneBases_) {
		if (cloneBase->IsHeld() || cloneBase->IsAnimating()) {
			continue;
		}

		const MapChipField::Rect cloneRect = cloneBase->GetRect();
		bool isTouchingSource = false;

		// 帯電ポイントとの接触
		for (const ChargePoint* chargePoint : chargePoints_) {
			const MapChipField::Rect contactRect = ExpandRect(chargePoint->GetRect(), kChargeContactMargin);

			if (IsRectColliding(cloneRect, contactRect)) {
				isTouchingSource = true;
				break;
			}
		}

		// 有効なレーザーとの接触
		if (!isTouchingSource) {
			for (const Lazer* lazer : lazers_) {
				if (!lazer->IsActive()) {
					continue;
				}

				if (IsRectColliding(cloneRect, lazer->GetRect())) {
					isTouchingSource = true;
					break;
				}
			}
		}

		// 帯電ポイントか有効なレーザーに触れている間は、
		// 毎フレームCharge()を呼んで残り時間を最大に保つ
		if (isTouchingSource) {
			ChargeClone(cloneBase);
		}

	}
}

void GameScene::UpdateElectricPlatforms() {
	for (ElectricPlatform* platform : electricPlatforms_) {
		const MapChipField::Rect previousRect = platform->GetRect();
		platform->Update();
		const Vector3 delta = platform->GetMoveDelta();

		if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) {
			continue;
		}

		const Vector3 playerPosition = player_->GetWorldTransform().translation_;
		const float playerHalfWidth = player_->GetWidth() / 2.0f;
		const float playerHalfHeight = player_->GetHeight() / 2.0f;
		const MapChipField::Rect playerRect = {
		    playerPosition.x - playerHalfWidth, playerPosition.x + playerHalfWidth,
		    playerPosition.y - playerHalfHeight, playerPosition.y + playerHalfHeight};

		const bool playerWasCarried = IsStandingOnRect(playerRect, previousRect);

		if (playerWasCarried) {
			player_->SetTranslation(playerPosition + delta);
		}

		for (CloneBase* cloneBase : cloneBases_) {
			if (cloneBase->IsHeld() || cloneBase->IsAnimating()) {
				continue;
			}

			const MapChipField::Rect cloneRect = cloneBase->GetRect();

			const bool onPlatform = IsStandingOnRect(cloneRect, previousRect);

			const bool onCarriedPlayer = playerWasCarried && IsStandingOnRect(cloneRect, playerRect);

			if (!onPlatform && !onCarriedPlayer) {
				continue;
			}

			if (cloneBase->GetState() == CloneBase::State::kTransformed) {
				Player* clonePlayer = cloneBase->GetPlayer();
				clonePlayer->SetTranslation(clonePlayer->GetWorldTransform().translation_ + delta);
			} else {
				cloneBase->SetTranslation(cloneBase->GetWorldTransform().translation_ + delta);
			}
		}
	}
}
