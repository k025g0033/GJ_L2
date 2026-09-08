#pragma once
#include "KamataEngine.h"
#include "Player.h"

#include <map>
#include <string>

class Player;

/// <summary>
/// カメラ制御
/// 自機やクローンには追従せず、ステージごとにCSVで決めた位置に固定する。
/// スクロールを有効にしたステージだけ、その軸（X/Y）が自機の座標に合わせて動く。
/// </summary>
class CameraController {
public:
	///// ----- ステージごとのカメラ設定 ----- /////
	// Resources/map/camera.csv の1行ぶん
	struct StageSetting {
		// カメラの基本位置（スクロールを有効にした軸では、この値は使わずに自機基準で決まる）
		KamataEngine::Vector3 position = {11.0f, 6.0f, -15.0f};
		// カメラの角度（度）。ImGuiで扱いやすいようラジアンではなく度で持つ。
		KamataEngine::Vector3 rotationDegree = {0.0f, 0.0f, 0.0f};

		/// --- 横スクロール（マリオのように自機に合わせてステージが横に流れる） ---
		// 横スクロールするか
		bool isScrollX = false;
		// 自機のX座標に対するカメラのずれ。マイナスにすると自機が画面の右寄りになる。
		float scrollOffsetX = 0.0f;
		// カメラX座標の下限・上限（下限 >= 上限 の場合は制限しない）
		float scrollMinX = 0.0f;
		float scrollMaxX = 0.0f;

		/// --- 縦スクロール（自機の上下移動に合わせてカメラも縦に動く） ---
		// 縦スクロールするか
		bool isScrollY = false;
		// 自機のY座標に対するカメラのずれ。マイナスにすると自機が画面の上寄りになる。
		float scrollOffsetY = 0.0f;
		// カメラY座標の下限・上限（下限 >= 上限 の場合は制限しない）
		float scrollMinY = 0.0f;
		float scrollMaxY = 0.0f;
	};

	void Initialize(KamataEngine::Camera* camera);

	void Update();

	// カメラを設定位置へ即座に合わせる（ステージ開始時に使う）
	void Reset();

	///// ----- ステージ設定の読み書き ----- /////
	// CSVから全ステージぶんのカメラ設定を読み込む。ファイルが無くても落ちず、既定値のままになる。
	void LoadStageSettingsCsv(const std::string& filePath);
	// 今の設定を全ステージぶんCSVへ書き出す（ImGuiで調整した結果を保存する）
	bool SaveStageSettingsCsv(const std::string& filePath) const;

	// 使うステージを選ぶ。設定が無いステージは既定値で作られる。
	void SelectStage(int stageNumber);
	int GetStageNumber() const { return stageNumber_; }

	// 今のステージの設定（ImGuiから直接書き換えられるよう参照を返す）
	StageSetting& GetCurrentSettingRef();

	// スクロールの基準にするキャラ。スクロールを有効にした軸の座標だけを見る。
	// ※クローンではなく自機を渡すこと（クローンを操作中でもカメラは自機基準のまま）
	void SetTarget(const Player* target) { target_ = target; }

private:
	// 今のステージ設定を読み取り専用で取得する
	const StageSetting& GetCurrentSetting() const;

	// 設定から、このフレームのカメラ位置を求める
	KamataEngine::Vector3 CalculateCameraPosition() const;

	// カメラ
	KamataEngine::Camera* camera_ = nullptr;

	// スクロールの基準にする自機
	const Player* target_ = nullptr;

	// ステージ番号 -> カメラ設定
	std::map<int, StageSetting> stageSettings_;
	// 今選ばれているステージ番号
	int stageNumber_ = 1;

	// 設定が見つからない時に返す既定値
	StageSetting defaultSetting_;

	// スクロールの追従のなめらかさ（1.0で即座に追いつく）
	static inline const float kScrollInterpolationRate = 0.2f;
};
