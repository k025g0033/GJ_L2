#pragma once
#include "BackGround.h"
#include "CameraController.h"
#include "CloneBase.h"
#include "Door.h"
#include "IScene.h"
#include "KamataEngine.h"
#include "Lazer.h"
#include "Line3D.h"
#include "MapChipField.h"
#include "MouseCursor.h"
#include "Player.h"
#include "PushPlate.h"
#include "Skydome.h"
#include "ThrowAimIndicator.h"
#include <vector>
#include "Key.h"
#include "ElectricBullet.h"
#include "ChargePoint.h"
#include "ElectricPlatform.h"
#include <array>

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
	bool GetReloadRequested() const override { return reloadRequested_; }
	bool GetStageSelectRequested() const override { return stageSelectRequested_; }

	void GenerateBlocks();

	// 全ての当たり判定を行う
	void CheckAllCollisions();

private:
	std::vector<PushPlate*> pressurePlates_;
	std::vector<Door*> doors_;
	std::vector<Key*> keys_;


	void UpdatePressurePlates();
	void UpdateDoors();
	void UpdateLazers();
	void UpdateKeys(Player* activePlayer);
	void StartGoalCameraCinematic(uint8_t keyID);
	void UpdateGoalCameraCinematic();
	// ゴール判定（渡されたキャラだけを対象にする。自機のみを渡すこと）
	void CheckDoorGoal(const Player* goalPlayer);

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
	// 電動足場モデル
	KamataEngine::Model* modelElectricPlatform_ = nullptr;
	// エネルギー源モデル
	KamataEngine::Model* modelChargePoint_ = nullptr;
	// ドアモデル
	KamataEngine::Model* modelDoor_ = nullptr;
	KamataEngine::Model* modelDoorOpen_ = nullptr;
	KamataEngine::Model* modelDoorOpenGlass_ = nullptr;
	// 感圧板モデル（土台・押下部分）
	KamataEngine::Model* modelPushPlateBase_ = nullptr;
	KamataEngine::Model* modelPushPlateButton_ = nullptr;
	// レーザーモデル
	KamataEngine::Model* modelLazer_ = nullptr;
	// 天球モデル
	KamataEngine::Model* modelSkydome_ = nullptr;
	// 水モデル
	KamataEngine::Model* modelWater_ = nullptr;
	// クローンの素モデル（Resources/cloneObject）
	KamataEngine::Model* modelCloneBase_ = nullptr;
	// 鍵モデル
	KamataEngine::Model* modelKey_ = nullptr;
	// 自機の追加パーツモデル（頭・左腕・右腕）。ベースモデルに重ねて描画する。
	KamataEngine::Model* modelPlayerHead_ = nullptr;
	KamataEngine::Model* modelPlayerLeftArm_ = nullptr;
	KamataEngine::Model* modelPlayerRightArm_ = nullptr;
	// クローンの素を持っている間だけ使うパーツ（腕を上げた形＋抱えているクローン）
	KamataEngine::Model* modelPlayerLeftArmHolding_ = nullptr;
	KamataEngine::Model* modelPlayerRightArmHolding_ = nullptr;
	KamataEngine::Model* modelPlayerHoldingClone_ = nullptr;
	Line3D* line3D_ = nullptr;
	// 電撃弾モデル
	std::vector<ElectricBullet*> electricBullets_;
	KamataEngine::Model* modelElectricBullet_ = nullptr;
	// ステージごとの背景画像と、右から左へ流れる雲。
	BackGround* background_ = nullptr;

	// ポーズ表示
	bool isPaused_ = false;
	uint32_t pauseEscTextureHandle_ = 0;
	uint32_t pausePoseTextureHandle_ = 0;
	uint32_t pauseOverlayTextureHandle_ = 0;
	KamataEngine::Sprite* pauseEscSprite_ = nullptr;
	KamataEngine::Sprite* pausePoseGuideSprite_ = nullptr;
	KamataEngine::Sprite* pauseOverlaySprite_ = nullptr;
	KamataEngine::Sprite* pauseTitleSprite_ = nullptr;
	std::array<uint32_t, 4> pauseMenuTextureHandles_{};
	std::array<KamataEngine::Sprite*, 4> pauseMenuSprites_{};
	int selectedPauseItem_ = 0;
	float pauseSelectionAnimationTime_ = 0.0f;
	uint32_t cursorMoveSoundHandle_ = 0;
	uint32_t decideSoundHandle_ = 0;
	uint32_t electricChargeSoundHandle_ = 0;
	uint32_t electricFireSoundHandle_ = 0;
	uint32_t waterSplashSoundHandle_ = 0;
	uint32_t keyGetSoundHandle_ = 0;
	uint32_t pushPlateSoundHandle_ = 0;
	uint32_t controlSwitchSoundHandle_ = 0;
	uint32_t cloneLandingSoundHandle_ = 0;
	uint32_t jumpSoundHandle_ = 0;
	bool reloadRequested_ = false;
	bool stageSelectRequested_ = false;

	std::vector<ChargePoint*> chargePoints_;
	std::vector<ElectricPlatform*> electricPlatforms_;

	// ブロック用ワールドトランスフォーム
	std::vector<std::vector<KamataEngine::WorldTransform*>> worldTransformBlocks_;
	// 水用ワールドトランスフォーム
	std::vector<std::vector<KamataEngine::WorldTransform*>> worldTransformWaters_;

	// カメラ
	KamataEngine::Camera camera_;

	// 鍵取得時のゴール紹介カメラ
	enum class GoalCameraPhase {
		kFocus,
		kFocusHold,
		kReturn,
	};
	bool isGoalCameraCinematic_ = false;
	GoalCameraPhase goalCameraPhase_ = GoalCameraPhase::kFocus;
	float goalCameraTimer_ = 0.0f;
	uint8_t goalCameraDoorID_ = 0;
	KamataEngine::Vector3 goalCameraStart_{};
	KamataEngine::Vector3 goalCameraFocus_{};
	static inline const float kGoalCameraFocusDuration = 0.6f;
	static inline const float kGoalCameraFocusHoldDuration = 0.75f;
	static inline const float kGoalCameraReturnDuration = 0.7f;
	// ドアを画面中央より少し下へ置き、床下が見えないようにする高さ。
	static inline const float kGoalCameraFloorViewOffsetY = 2.5f;
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

	// ImGui上でこのステージのカメラ設定を調整するパネルを表示する
	void ShowCameraImGui();

	// 自機・クローン・クローンの素の当たり判定をワイヤーフレームの箱で可視化する
	// （Debugビルド/USE_IMGUI時のみ）
	void DrawCollisionWireframes();
	// 矩形1つぶんのワイヤーフレームの箱を描く
	void DrawRectWireframe(const MapChipField::Rect& rect, float centerZ, float halfDepth, const KamataEngine::Vector4& color);

	// カメラ設定の保存結果をImGuiに出すための文字列
	const char* cameraSaveMessage_ = "";

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

	///// ----- クローンの素を投げる処理 ----- /////
	// マウスカーソル方向へ、持っているクローンの素を投げる
	void ThrowHeldCloneBase();

	// マウスカーソル表示（AL3_評価課題02から流用）
	MouseCursor* mouseCursor_ = nullptr;

	// 投げる力（距離に関わらず常に一定。ImGuiで調整可能）
	float throwPower_ = 0.3f;
	// 投げる力の上限（ImGuiスライダーの範囲用）
	static inline const float kMaxThrowPower = 0.6f;

	// クローンの素を持っている間だけ表示する、投げる方向を示すUI（円＋三角形）
	ThrowAimIndicator* throwAimIndicator_ = nullptr;

	///// ----- クローン帯電関連 ----- /////
	// 未帯電から帯電へ切り替わった時だけSEを鳴らす
	void ChargeClone(CloneBase* cloneBase);
	// 電気弾発射
	void FireElectricBullet();

	/// --- 帯電の受け渡し ---
	// 帯電しているクローンが、帯電していないクローンの素に触れた時に電気を移す
	void UpdateChargeTransfer();
	// 全員が帯電を使い終わっていたら、帯電履歴をまとめてリセットする
	void ResetChargeHistoryIfAllUsed();

	void UpdateChargeSources();
	void UpdateElectricPlatforms();

	float chargeEffectTime_ = 0.0f;

	int stageNumber_ = 1;
};
