#pragma once
#include "KamataEngine.h"
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

// 背景と雲を、ステージごとの画面内の位置で配置する。
class BackGround {
public:
	struct StageSetting {
		std::string image = "images/BackGround/01.png";
		// 1280x720の画面座標。位置は画像の中心、Yは下向きが正。
		float x = 640.0f;
		float y = 360.0f;
		float width = 1280.0f;
		float height = 720.0f;
		// ワールドZではなく、カメラから前方への距離。
		float distance = 80.0f;
		bool cloudEnabled = true;
		float cloudStartX = 1500.0f;
		float cloudYMin = 60.0f;
		float cloudYMax = 200.0f;
		float cloudDistance = 40.0f;
		float cloudScale = 0.7f;
		float cloudSpeed = 45.0f; // 画面上のピクセル/秒。正の値で左へ流れる。
		float cloudIntervalMin = 2.0f; // 生成するたびに次の待ち時間を抽選する（秒）。
		float cloudIntervalMax = 6.0f;
		float cloudEndX = -220.0f;
		int cloudInitialCount = 3;
		int cloudSpawnCountMin = 1;
		int cloudSpawnCountMax = 2;
		int cloudMaxCount = 12; // 画面外で移動中の雲も含めた同時存在数。
		float cloudYawMin = 0.0f; // Y軸回転の範囲（度）。生成時にだけ向きを決める。
		float cloudYawMax = 360.0f;
	};

	void Initialize(KamataEngine::Camera* camera, int stageNumber);
	void Update();
	// 描画パスの開始・終了はGameScene側で行う。
	void DrawBackground();
	void DrawClouds();
	void ShowImGui();

private:
	struct Cloud {
		KamataEngine::WorldTransform transform;
		float x = 0.0f;
		float y = 0.0f;
	};
	bool LoadCsv();
	bool SaveCsv();
	bool Validate(const StageSetting& setting) const;
	void ResetClouds();
	void SpawnCloud(float x);
	float RandomFloat(float minimum, float maximum);
	// 実際に描画するカメラの行列を使うので、移動・回転・デバッグカメラにも追従する。
	void PlaceInCamera(KamataEngine::WorldTransform& transform, float x, float y, float distance, const KamataEngine::Vector3& scale);

	KamataEngine::Camera* camera_ = nullptr;
	int stageNumber_ = 1;
	std::map<int, StageSetting> settings_;
	std::unique_ptr<KamataEngine::LightGroup> backgroundLight_;
	std::unique_ptr<KamataEngine::Model> model_;
	std::unique_ptr<KamataEngine::Model> cloudModel_;
	KamataEngine::WorldTransform transform_;
	uint32_t textureHandle_ = 0;
	std::vector<std::unique_ptr<Cloud>> clouds_;
	float spawnTimer_ = 0.0f;
	std::mt19937 randomEngine_{std::random_device{}()};
	std::string message_;
};
