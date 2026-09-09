#include "BackGround.h"
#include "2d/ImGuiManager.h"
#include "math/MathUtility.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

namespace {
const char* kStageDataPath = "Resources/map/StageData.csv";
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;
constexpr float kDeltaTime = 1.0f / 60.0f;
// 背景のbgX・bgY・bgWidth・bgHeightを画面座標として解釈するときの基準距離。
// bgDistanceがこの値のとき、CSVで指定した画面上の位置とサイズがそのまま出る。
constexpr float kBackgroundBaseDistance = 80.0f;
// 誤設定で雲が増え続けないよう、同時表示数に上限を設ける。
constexpr size_t kMaxClouds = 64;

std::string Trim(const std::string& value) {
	const size_t first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) { return ""; }
	return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// 数字の後ろに文字が混じった行や、NaNなども設定エラーとして扱う。
float ReadNumber(const std::string& word) {
	size_t used = 0;
	const float value = std::stof(word, &used);
	if (used != word.size() || !std::isfinite(value)) { throw std::invalid_argument("Invalid number"); }
	return value;
}

int ReadCount(const std::string& word) {
	const float count = ReadNumber(word);
	if (count < 0.0f || count > static_cast<float>(kMaxClouds) || std::floor(count) != count) {
		throw std::invalid_argument("Invalid cloud count");
	}
	return static_cast<int>(count);
}
}

void BackGround::Initialize(Camera* camera, int stageNumber) {
	assert(camera);
	camera_ = camera;
	stageNumber_ = stageNumber;
	model_.reset(Model::CreateFromOBJ("BackGround"));
	cloudModel_.reset(Model::CreateFromOBJ("cloud", true));
	transform_.Initialize();

	// 描いた背景の色を保つため、板だけ環境光100%で描画する。
	backgroundLight_.reset(LightGroup::Create());
	backgroundLight_->SetAmbientColor({1.0f, 1.0f, 1.0f});
	for (int i = 0; i < LightGroup::kDirLightNum; ++i) { backgroundLight_->SetDirLightActive(i, false); }
	for (int i = 0; i < LightGroup::kPointLightNum; ++i) { backgroundLight_->SetPointLightActive(i, false); }
	for (int i = 0; i < LightGroup::kSpotLightNum; ++i) { backgroundLight_->SetSpotLightActive(i, false); }
	for (int i = 0; i < LightGroup::kCircleShadowNum; ++i) { backgroundLight_->SetCircleShadowActive(i, false); }
	backgroundLight_->Update();
	model_->SetLightGroup(backgroundLight_.get());
	settings_[stageNumber_] = StageSetting{};
	textureHandle_ = TextureManager::Load(settings_.at(stageNumber_).image);
	if (!LoadCsv()) { ResetClouds(); }
}

bool BackGround::Validate(const StageSetting& s) const {
	std::error_code error;
	return std::filesystem::is_regular_file(std::filesystem::path("Resources") / s.image, error) &&
	    s.width > 0.0f && s.height > 0.0f && s.distance > camera_->nearZ && s.distance < camera_->farZ &&
	    s.cloudDistance > camera_->nearZ && s.cloudDistance < s.distance && s.cloudScale > 0.0f &&
	    s.cloudSpeed > 0.0f && s.cloudIntervalMin >= kDeltaTime && s.cloudIntervalMax >= s.cloudIntervalMin &&
	    s.cloudYMax >= s.cloudYMin && s.cloudYawMax >= s.cloudYawMin && s.cloudStartX > s.cloudEndX &&
	    s.cloudMaxCount >= 1 && s.cloudMaxCount <= static_cast<int>(kMaxClouds) &&
	    s.cloudInitialCount >= 0 && s.cloudInitialCount <= s.cloudMaxCount &&
	    s.cloudSpawnCountMin >= 1 && s.cloudSpawnCountMax >= s.cloudSpawnCountMin && s.cloudSpawnCountMax <= s.cloudMaxCount;
}

bool BackGround::LoadCsv() {
	std::ifstream file(kStageDataPath);
	if (!file) { message_ = "Cannot open StageData.csv (current settings kept)."; return false; }
	std::map<int, StageSetting> loaded;
	std::string line;
	int lineNumber = 0;
	try {
		while (std::getline(file, line)) {
			++lineNumber;
			// Excelなどで付くUTF-8 BOMは最初の行だけ取り除く。
			if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) { line.erase(0, 3); }
			line = Trim(line);
			if (line.empty() || line[0] == '#') { continue; }
			std::istringstream stream(line);
			std::vector<std::string> cells;
			std::string cell;
			while (std::getline(stream, cell, ',')) { cells.push_back(Trim(cell)); }
			// 旧16列も読み込めるようにし、ユーザーが調整した位置やサイズを引き継ぐ。
			if (cells.size() != 16 && cells.size() != 23) { throw std::invalid_argument("Expected 16 or 23 columns"); }
			const float stageValue = ReadNumber(cells[0]);
			if (stageValue < 1.0f || stageValue > 9999.0f || std::floor(stageValue) != stageValue) { throw std::invalid_argument("Invalid stage"); }
			const int stage = static_cast<int>(stageValue);
			StageSetting s;
			s.image = cells[1];
			s.x = ReadNumber(cells[2]); s.y = ReadNumber(cells[3]);
			s.width = ReadNumber(cells[4]); s.height = ReadNumber(cells[5]); s.distance = ReadNumber(cells[6]);
			const float enabled = ReadNumber(cells[7]);
			if (enabled != 0.0f && enabled != 1.0f) { throw std::invalid_argument("Invalid enabled flag"); }
			s.cloudEnabled = enabled == 1.0f;
			s.cloudStartX = ReadNumber(cells[8]); s.cloudYMin = ReadNumber(cells[9]); s.cloudDistance = ReadNumber(cells[10]);
			s.cloudScale = ReadNumber(cells[11]); s.cloudSpeed = ReadNumber(cells[12]); s.cloudIntervalMin = ReadNumber(cells[13]);
			s.cloudEndX = ReadNumber(cells[14]);
			s.cloudInitialCount = ReadCount(cells[15]);
			if (cells.size() == 23) {
				s.cloudYMax = ReadNumber(cells[16]); s.cloudIntervalMax = ReadNumber(cells[17]);
				s.cloudSpawnCountMin = ReadCount(cells[18]); s.cloudSpawnCountMax = ReadCount(cells[19]);
				s.cloudMaxCount = ReadCount(cells[20]);
				s.cloudYawMin = ReadNumber(cells[21]); s.cloudYawMax = ReadNumber(cells[22]);
			} else {
				// 旧CSVの固定値は最小=最大として扱う。保存すると新しい23列になる。
				s.cloudYMax = s.cloudYMin; s.cloudIntervalMax = s.cloudIntervalMin;
				s.cloudSpawnCountMin = s.cloudSpawnCountMax = 1;
				s.cloudMaxCount = static_cast<int>(kMaxClouds);
			}
			if (loaded.contains(stage) || !Validate(s)) { throw std::invalid_argument("Invalid setting or missing image"); }
			loaded.emplace(stage, s);
		}
		if (!file.eof()) { throw std::invalid_argument("Read failed"); }
		if (!loaded.contains(stageNumber_)) { throw std::invalid_argument("Current stage is missing"); }
	} catch (const std::exception&) {
		message_ = "StageData.csv error near line " + std::to_string(lineNumber) + " (current settings kept).";
		return false;
	}
	// 全行を確認してから切り替えるので、編集途中のCSVでは現在の表示を壊さない。
	settings_ = std::move(loaded);
	textureHandle_ = TextureManager::Load(settings_.at(stageNumber_).image);
	ResetClouds();
	message_ = "Loaded StageData.csv";
	return true;
}

bool BackGround::SaveCsv() {
	for (const auto& [stage, s] : settings_) {
		(void)stage;
		if (!Validate(s)) { message_ = "Invalid settings. Check size, distance and cloud range."; return false; }
	}
	std::ofstream file(kStageDataPath);
	if (!file) { message_ = "Cannot save StageData.csv"; return false; }
	file << "# 背景と雲のステージ設定。座標・単位の説明はStageDataMemo.txtを参照。\n"
	     << "# stage,image,bgX,bgY,bgWidth,bgHeight,bgDistance,cloudEnabled,cloudStartX,cloudYMin,cloudDistance,cloudScale,cloudSpeed,cloudIntervalMin,cloudEndX,cloudInitialCount,cloudYMax,cloudIntervalMax,cloudSpawnCountMin,cloudSpawnCountMax,cloudMaxCount,cloudYawMin,cloudYawMax\n";
	for (const auto& [stage, s] : settings_) {
		file << stage << ',' << s.image << ',' << s.x << ',' << s.y << ',' << s.width << ',' << s.height << ',' << s.distance << ','
		     << (s.cloudEnabled ? 1 : 0) << ',' << s.cloudStartX << ',' << s.cloudYMin << ',' << s.cloudDistance << ',' << s.cloudScale << ','
		     << s.cloudSpeed << ',' << s.cloudIntervalMin << ',' << s.cloudEndX << ',' << s.cloudInitialCount << ','
		     << s.cloudYMax << ',' << s.cloudIntervalMax << ',' << s.cloudSpawnCountMin << ',' << s.cloudSpawnCountMax << ','
		     << s.cloudMaxCount << ',' << s.cloudYawMin << ',' << s.cloudYawMax << '\n';
	}
	file.close();
	message_ = file ? "Saved StageData.csv" : "Failed to save StageData.csv";
	return static_cast<bool>(file);
}

float BackGround::RandomFloat(float minimum, float maximum) {
	return std::uniform_real_distribution<float>(minimum, maximum)(randomEngine_);
}

void BackGround::SpawnCloud(float x) {
	const StageSetting& s = settings_.at(stageNumber_);
	if (clouds_.size() >= static_cast<size_t>(s.cloudMaxCount)) { return; }
	auto cloud = std::make_unique<Cloud>();
	cloud->transform.Initialize();
	cloud->x = x;
	// 高さと向きは雲ごとに保持し、毎フレーム抽選してちらつかないようにする。
	cloud->y = RandomFloat(s.cloudYMin, s.cloudYMax);
	cloud->transform.rotation_.y = RandomFloat(s.cloudYawMin, s.cloudYawMax) * std::numbers::pi_v<float> / 180.0f;
	clouds_.push_back(std::move(cloud));
}

void BackGround::ResetClouds() {
	clouds_.clear();
	const StageSetting& s = settings_.at(stageNumber_);
	spawnTimer_ = RandomFloat(s.cloudIntervalMin, s.cloudIntervalMax);
	if (!s.cloudEnabled) { return; }
	for (int i = 0; i < s.cloudInitialCount; ++i) {
		// 初期配置も等間隔を避け、指定した移動範囲から位置を抽選する。
		SpawnCloud(RandomFloat(s.cloudEndX, s.cloudStartX));
	}
}

void BackGround::Update() {
	const StageSetting& s = settings_.at(stageNumber_);
	if (!s.cloudEnabled) { clouds_.clear(); return; }
	// 調整中に上限を下げた場合も、そのフレームから上限を守る。
	if (clouds_.size() > static_cast<size_t>(s.cloudMaxCount)) { clouds_.resize(static_cast<size_t>(s.cloudMaxCount)); }
	for (auto& cloud : clouds_) { cloud->x -= s.cloudSpeed * kDeltaTime; }
	// 参考フォルダと同様に、左へ抜けた雲を解放してから右側に新しい雲を生成する。
	std::erase_if(clouds_, [&s](const auto& cloud) { return cloud->x < s.cloudEndX; });
	spawnTimer_ -= kDeltaTime;
	if (spawnTimer_ <= 0.0f) {
		const int count = std::uniform_int_distribution<int>(s.cloudSpawnCountMin, s.cloudSpawnCountMax)(randomEngine_);
		for (int i = 0; i < count; ++i) { SpawnCloud(s.cloudStartX); }
		spawnTimer_ = RandomFloat(s.cloudIntervalMin, s.cloudIntervalMax);
	}
}

void BackGround::PlaceInCamera(WorldTransform& transform, float x, float y, float distance, const Vector3& scale, float layoutDistance) {
	// 換算に使う距離。基準距離を渡せば、実際の奥行きとは切り離して画面座標を解釈できる。
	const float baseDistance = layoutDistance > 0.0f ? layoutDistance : distance;
	// 射影行列から、その奥行きで画面に入るワールド幅・高さを逆算する。
	const float viewWidth = 2.0f * baseDistance / camera_->matProjection.m[0][0];
	const float viewHeight = 2.0f * baseDistance / camera_->matProjection.m[1][1];
	transform.scale_ = scale;
	transform.translation_ = {(x / kScreenWidth - 0.5f) * viewWidth, (0.5f - y / kScreenHeight) * viewHeight, distance};
	// カメラ基準の配置をワールド座標へ戻す。カメラ移動時も画像の同じ範囲が見える。
	transform.matWorld_ = MakeScaleMatrix(scale) * MakeRotateYMatrix(transform.rotation_.y) * MakeTranslateMatrix(transform.translation_) * Inverse(camera_->matView);
	transform.TransferMatrix();
}

void BackGround::DrawBackground() {
	const StageSetting& s = settings_.at(stageNumber_);
	// 大きさと位置は基準距離で一度だけワールドへ換算し、その板を実際の奥行きへ置く。
	// これでbgDistanceを大きくするほど小さく、画面中央寄りに見えるようになる。
	const float width = 2.0f * kBackgroundBaseDistance / camera_->matProjection.m[0][0] * s.width / kScreenWidth;
	const float height = 2.0f * kBackgroundBaseDistance / camera_->matProjection.m[1][1] * s.height / kScreenHeight;
	PlaceInCamera(transform_, s.x, s.y, s.distance, {width, height, 1.0f}, kBackgroundBaseDistance);
	model_->Draw(transform_, *camera_, textureHandle_);
}

void BackGround::DrawClouds() {
	const StageSetting& s = settings_.at(stageNumber_);
	if (!s.cloudEnabled) { return; }
	for (auto& cloud : clouds_) {
		PlaceInCamera(cloud->transform, cloud->x, cloud->y, s.cloudDistance, {s.cloudScale, s.cloudScale, s.cloudScale});
		cloudModel_->Draw(cloud->transform, *camera_);
	}
}

void BackGround::ShowImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Stage Background / Clouds");
	ImGui::Text("Stage: %d | coordinates: 1280 x 720", stageNumber_);
	StageSetting& s = settings_.at(stageNumber_);
	const StageSetting previous = s;
	ImGui::Text("Image: %s", s.image.c_str());
	ImGui::Text("Change image path in StageData.csv, then Reload.");
	ImGui::DragFloat("Background Center X", &s.x, 1.0f);
	ImGui::DragFloat("Background Center Y", &s.y, 1.0f);
	ImGui::DragFloat("Background Width", &s.width, 1.0f, 1.0f, 10000.0f);
	ImGui::DragFloat("Background Height", &s.height, 1.0f, 1.0f, 10000.0f);
	ImGui::DragFloat("Background Distance", &s.distance, 0.5f, 1.0f, 900.0f);
	ImGui::Separator();
	ImGui::Checkbox("Cloud Enabled", &s.cloudEnabled);
	ImGui::DragFloat("Cloud Start X", &s.cloudStartX, 1.0f);
	ImGui::DragFloat("Cloud Y Min", &s.cloudYMin, 1.0f);
	ImGui::DragFloat("Cloud Y Max", &s.cloudYMax, 1.0f);
	ImGui::DragFloat("Cloud Distance", &s.cloudDistance, 0.5f, 1.0f, 900.0f);
	ImGui::DragFloat("Cloud Model Scale", &s.cloudScale, 0.01f, 0.01f, 10.0f);
	ImGui::DragFloat("Cloud Speed (px/sec)", &s.cloudSpeed, 1.0f, 1.0f, 1000.0f);
	ImGui::DragFloat("Cloud Interval Min (sec)", &s.cloudIntervalMin, 0.1f, 0.1f, 60.0f);
	ImGui::DragFloat("Cloud Interval Max (sec)", &s.cloudIntervalMax, 0.1f, 0.1f, 60.0f);
	ImGui::DragFloat("Cloud End X", &s.cloudEndX, 1.0f);
	ImGui::DragInt("Cloud Initial Count", &s.cloudInitialCount, 1.0f, 0, static_cast<int>(kMaxClouds));
	ImGui::DragInt("Cloud Spawn Count Min", &s.cloudSpawnCountMin, 1.0f, 1, static_cast<int>(kMaxClouds));
	ImGui::DragInt("Cloud Spawn Count Max", &s.cloudSpawnCountMax, 1.0f, 1, static_cast<int>(kMaxClouds));
	ImGui::DragInt("Cloud Max Alive", &s.cloudMaxCount, 1.0f, 1, static_cast<int>(kMaxClouds));
	ImGui::DragFloat("Cloud Yaw Min (deg)", &s.cloudYawMin, 1.0f, -360.0f, 360.0f);
	ImGui::DragFloat("Cloud Yaw Max (deg)", &s.cloudYawMax, 1.0f, -360.0f, 360.0f);
	ImGui::Text("New clouds use new ranges. Restart Clouds reapplies all settings.");
	if (!Validate(s)) { s = previous; message_ = "Invalid change: check distance and range."; }
	ImGui::Text("Clouds: %zu / %d | Next: %.2f sec", clouds_.size(), s.cloudMaxCount, spawnTimer_);
	if (ImGui::Button("Restart Clouds")) { ResetClouds(); }
	ImGui::SameLine();
	if (ImGui::Button("Save StageData.csv")) { SaveCsv(); }
	ImGui::SameLine();
	if (ImGui::Button("Reload StageData.csv")) { LoadCsv(); }
	ImGui::TextWrapped("%s", message_.c_str());
	ImGui::End();
#endif
}
