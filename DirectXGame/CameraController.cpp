#define NOMINMAX
#include "CameraController.h"
#include "KamataEngine.h"
#include "Player.h"
#include "math/MathUtility.h"

#include <algorithm>
#include <fstream>
#include <numbers>
#include <sstream>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

namespace {

// 度をラジアンに変換する
float ToRadian(float degree) { return degree * std::numbers::pi_v<float> / 180.0f; }

// 文字列の前後の空白を取り除く
std::string Trim(const std::string& text) {
	size_t begin = text.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos) {
		return "";
	}
	size_t end = text.find_last_not_of(" \t\r\n");
	return text.substr(begin, end - begin + 1);
}

// カンマ区切りの1行を項目ごとに分解する
std::vector<std::string> SplitCsvLine(const std::string& line) {
	std::vector<std::string> words;
	std::istringstream lineStream(line);
	std::string word;
	while (std::getline(lineStream, word, ',')) {
		words.push_back(Trim(word));
	}
	return words;
}

// 文字列を数値に変換する（変換できなければ既定値をそのまま返す）
float ToFloat(const std::string& text, float defaultValue) {
	if (text.empty()) {
		return defaultValue;
	}
	try {
		return std::stof(text);
	} catch (...) {
		return defaultValue;
	}
}

} // namespace

void CameraController::Initialize(Camera* camera) {
	// カメラの初期化
	camera_ = camera;
}

///// ----- 更新 ----- /////
// 自機やクローンには追従しない。ステージ設定で決めた位置に置くだけ。
// スクロールが有効な軸だけ、自機の座標に合わせてカメラを動かす。
void CameraController::Update() {
	if (camera_ == nullptr) {
		return;
	}

	const StageSetting& setting = GetCurrentSetting();
	const Vector3 targetPosition = CalculateCameraPosition();

	// スクロールする軸だけゆるやかに追いかける（固定の軸は即座に合わせる）
	if (setting.isScrollX) {
		camera_->translation_.x += (targetPosition.x - camera_->translation_.x) * kScrollInterpolationRate;
	} else {
		camera_->translation_.x = targetPosition.x;
	}

	if (setting.isScrollY) {
		camera_->translation_.y += (targetPosition.y - camera_->translation_.y) * kScrollInterpolationRate;
	} else {
		camera_->translation_.y = targetPosition.y;
	}

	// 奥行きは常に固定
	camera_->translation_.z = targetPosition.z;

	// 角度は度で持っているのでラジアンに直して反映する
	camera_->rotation_ = {
	    ToRadian(setting.rotationDegree.x), ToRadian(setting.rotationDegree.y), ToRadian(setting.rotationDegree.z)};

	// 行列を更新する
	camera_->UpdateMatrix();
}

///// ----- ステージ開始時の位置合わせ ----- /////
void CameraController::Reset() {
	if (camera_ == nullptr) {
		return;
	}

	const StageSetting& setting = GetCurrentSetting();

	// スクロールするステージでも、開始時は補間せずいきなり目標位置へ置く
	camera_->translation_ = CalculateCameraPosition();
	camera_->rotation_ = {
	    ToRadian(setting.rotationDegree.x), ToRadian(setting.rotationDegree.y), ToRadian(setting.rotationDegree.z)};
	camera_->UpdateMatrix();
}

///// ----- カメラ位置の計算 ----- /////
Vector3 CameraController::CalculateCameraPosition() const {
	const StageSetting& setting = GetCurrentSetting();

	Vector3 position = setting.position;

	if (target_ == nullptr) {
		return position;
	}

	const Vector3& targetPosition = target_->GetWorldTransform().translation_;

	// 横スクロールするステージは、自機のX座標に合わせてカメラのXを決める
	if (setting.isScrollX) {
		position.x = targetPosition.x + setting.scrollOffsetX;

		// 下限 < 上限 のときだけ範囲制限をかける（両方0のままなら制限なし）
		if (setting.scrollMinX < setting.scrollMaxX) {
			position.x = std::clamp(position.x, setting.scrollMinX, setting.scrollMaxX);
		}
	}

	// 縦スクロールするステージは、自機のY座標に合わせてカメラのYを決める
	if (setting.isScrollY) {
		position.y = targetPosition.y + setting.scrollOffsetY;

		if (setting.scrollMinY < setting.scrollMaxY) {
			position.y = std::clamp(position.y, setting.scrollMinY, setting.scrollMaxY);
		}
	}

	return position;
}

///// ----- ステージの選択 ----- /////
void CameraController::SelectStage(int stageNumber) {
	stageNumber_ = stageNumber;

	// 設定が無いステージは、ここで既定値のものを作っておく（ImGuiでそのまま調整・保存できる）
	if (!stageSettings_.contains(stageNumber_)) {
		stageSettings_[stageNumber_] = StageSetting{};
	}
}

CameraController::StageSetting& CameraController::GetCurrentSettingRef() {
	if (!stageSettings_.contains(stageNumber_)) {
		stageSettings_[stageNumber_] = StageSetting{};
	}
	return stageSettings_[stageNumber_];
}

const CameraController::StageSetting& CameraController::GetCurrentSetting() const {
	auto it = stageSettings_.find(stageNumber_);
	if (it == stageSettings_.end()) {
		return defaultSetting_;
	}
	return it->second;
}

///// ----- CSVの読み込み ----- /////
// 1行の書式：
//   stage, posX, posY, posZ, rotX, rotY, rotZ,
//   scrollX, scrollOffsetX, scrollMinX, scrollMaxX,
//   scrollY, scrollOffsetY, scrollMinY, scrollMaxY
// '#' で始まる行と空行はコメントとして読み飛ばす。
// 項目が足りない行は、足りないぶんが既定値のままになる（古い書式のCSVもそのまま読める）。
void CameraController::LoadStageSettingsCsv(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file.is_open()) {
		// ファイルが無くても落とさない。各ステージは既定値のまま扱う。
		return;
	}

	stageSettings_.clear();

	std::string line;
	while (std::getline(file, line)) {
		const std::string trimmed = Trim(line);
		if (trimmed.empty() || trimmed[0] == '#') {
			continue;
		}

		const std::vector<std::string> words = SplitCsvLine(trimmed);
		if (words.empty()) {
			continue;
		}

		int stageNumber = static_cast<int>(ToFloat(words[0], 0.0f));
		if (stageNumber <= 0) {
			continue;
		}

		StageSetting setting;
		if (words.size() > 1) setting.position.x = ToFloat(words[1], setting.position.x);
		if (words.size() > 2) setting.position.y = ToFloat(words[2], setting.position.y);
		if (words.size() > 3) setting.position.z = ToFloat(words[3], setting.position.z);
		if (words.size() > 4) setting.rotationDegree.x = ToFloat(words[4], setting.rotationDegree.x);
		if (words.size() > 5) setting.rotationDegree.y = ToFloat(words[5], setting.rotationDegree.y);
		if (words.size() > 6) setting.rotationDegree.z = ToFloat(words[6], setting.rotationDegree.z);
		if (words.size() > 7) setting.isScrollX = (ToFloat(words[7], 0.0f) != 0.0f);
		if (words.size() > 8) setting.scrollOffsetX = ToFloat(words[8], setting.scrollOffsetX);
		if (words.size() > 9) setting.scrollMinX = ToFloat(words[9], setting.scrollMinX);
		if (words.size() > 10) setting.scrollMaxX = ToFloat(words[10], setting.scrollMaxX);
		if (words.size() > 11) setting.isScrollY = (ToFloat(words[11], 0.0f) != 0.0f);
		if (words.size() > 12) setting.scrollOffsetY = ToFloat(words[12], setting.scrollOffsetY);
		if (words.size() > 13) setting.scrollMinY = ToFloat(words[13], setting.scrollMinY);
		if (words.size() > 14) setting.scrollMaxY = ToFloat(words[14], setting.scrollMaxY);

		stageSettings_[stageNumber] = setting;
	}
}

///// ----- CSVへの書き出し ----- /////
// ImGuiで調整した結果をそのままファイルへ保存する。
bool CameraController::SaveStageSettingsCsv(const std::string& filePath) const {
	std::ofstream file(filePath);
	if (!file.is_open()) {
		return false;
	}

	file << "# ステージごとのカメラ設定\n";
	file << "# stage,posX,posY,posZ,rotX,rotY,rotZ,"
	        "scrollX,scrollOffsetX,scrollMinX,scrollMaxX,"
	        "scrollY,scrollOffsetY,scrollMinY,scrollMaxY\n";
	file << "# rotX/Y/Zは角度(度)。scrollX/scrollYは1でその軸のスクロールあり、0でなし。\n";
	file << "# scrollOffsetX/Yは自機に対するカメラのずれ。Min >= Max なら範囲制限なし。\n";
	file << "# スクロールを有効にした軸では、posX/posYの値は使われない。\n";

	// std::mapなのでステージ番号順に並ぶ
	for (const auto& [stageNumber, setting] : stageSettings_) {
		file << stageNumber << ',' << setting.position.x << ',' << setting.position.y << ',' << setting.position.z << ','
		     << setting.rotationDegree.x << ',' << setting.rotationDegree.y << ',' << setting.rotationDegree.z << ','
		     << (setting.isScrollX ? 1 : 0) << ',' << setting.scrollOffsetX << ',' << setting.scrollMinX << ','
		     << setting.scrollMaxX << ',' << (setting.isScrollY ? 1 : 0) << ',' << setting.scrollOffsetY << ','
		     << setting.scrollMinY << ',' << setting.scrollMaxY << '\n';
	}

	return true;
}
