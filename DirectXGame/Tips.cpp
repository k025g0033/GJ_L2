#include "Tips.h"
#include "WorldTransformConfig.h"
#include "2d/ImGuiManager.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>

using namespace KamataEngine;

namespace {
const char* kTipsDataPath = "Resources/map/TipsData.csv";
// 画像を置いているフォルダ（Resourcesからの相対パス）
const char* kTipsImageDirectory = "images/tips";

std::string Trim(const std::string& value) {
	const size_t first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return "";
	}
	return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// 数字の後ろに文字が混じった行や、NaNなども設定エラーとして扱う。
float ReadNumber(const std::string& word) {
	size_t used = 0;
	const float value = std::stof(word, &used);
	if (used != word.size() || !std::isfinite(value)) {
		throw std::invalid_argument("Invalid number");
	}
	return value;
}
} // namespace

///// ----- 初期化 ----- /////

void Tips::Initialize(Camera* camera, int stageNumber) {
	assert(camera);
	camera_ = camera;
	stageNumber_ = stageNumber;

	// 板モデルは画像ごとにGetModel()で用意する（同じModelを使い回すと全部同じ絵になるため）。

	// 描いた絵の色を保つため、板だけ環境光100%で描画する。
	light_.reset(LightGroup::Create());
	light_->SetAmbientColor({1.0f, 1.0f, 1.0f});
	for (int i = 0; i < LightGroup::kDirLightNum; ++i) {
		light_->SetDirLightActive(i, false);
	}
	for (int i = 0; i < LightGroup::kPointLightNum; ++i) {
		light_->SetPointLightActive(i, false);
	}
	for (int i = 0; i < LightGroup::kSpotLightNum; ++i) {
		light_->SetSpotLightActive(i, false);
	}
	for (int i = 0; i < LightGroup::kCircleShadowNum; ++i) {
		light_->SetCircleShadowActive(i, false);
	}
	light_->Update();

	CollectImageFiles();
	LoadCsv();
}

///// ----- CSV読み込み ----- /////

bool Tips::LoadCsv() {
	std::ifstream file(kTipsDataPath);
	if (!file) {
		message_ = "Cannot open TipsData.csv (no tips shown).";
		return false;
	}

	std::map<int, std::vector<Board>> loaded;
	std::string line;
	int lineNumber = 0;

	try {
		while (std::getline(file, line)) {
			++lineNumber;
			// Excelなどで付くUTF-8 BOMは最初の行だけ取り除く。
			if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
				line.erase(0, 3);
			}
			line = Trim(line);
			if (line.empty() || line[0] == '#') {
				continue;
			}

			std::istringstream stream(line);
			std::vector<std::string> cells;
			std::string cell;
			while (std::getline(stream, cell, ',')) {
				cells.push_back(Trim(cell));
			}
			if (cells.size() != 8) {
				throw std::invalid_argument("Expected 8 columns");
			}

			const float stageValue = ReadNumber(cells[0]);
			if (stageValue < 1.0f || stageValue > 9999.0f || std::floor(stageValue) != stageValue) {
				throw std::invalid_argument("Invalid stage");
			}

			Board board;
			board.image = cells[1];
			board.position.x = ReadNumber(cells[2]);
			board.position.y = ReadNumber(cells[3]);
			board.position.z = ReadNumber(cells[4]);
			board.width = ReadNumber(cells[5]);
			board.height = ReadNumber(cells[6]);
			board.rotationDegreeY = ReadNumber(cells[7]);
			if (board.width <= 0.0f || board.height <= 0.0f) {
				throw std::invalid_argument("Invalid size");
			}

			loaded[static_cast<int>(stageValue)].push_back(board);
		}
	} catch (const std::exception&) {
		message_ = "TipsData.csv error near line " + std::to_string(lineNumber) + " (current settings kept).";
		return false;
	}

	// 全行を確認してから切り替えるので、編集途中のCSVでは現在の表示を壊さない。
	boards_ = std::move(loaded);
	selectedIndex_ = 0;
	message_ = "Loaded TipsData.csv";
	return true;
}

///// ----- CSV保存 ----- /////

bool Tips::SaveCsv() {
	std::ofstream file(kTipsDataPath);
	if (!file) {
		message_ = "Cannot save TipsData.csv";
		return false;
	}

	file << "# ステージごとのTIPS看板。1行が看板1枚で、同じステージ番号を何行書いてもよい。\n"
	     << "# 座標はワールド座標（ブロックと同じ空間）。widthとheightは1.0でブロック1マス分。\n"
	     << "# stage,image,posX,posY,posZ,width,height,rotationY\n";

	for (const auto& [stage, boards] : boards_) {
		for (const Board& board : boards) {
			file << stage << ',' << board.image << ',' << board.position.x << ',' << board.position.y << ','
			     << board.position.z << ',' << board.width << ',' << board.height << ',' << board.rotationDegreeY
			     << '\n';
		}
	}

	file.close();
	message_ = file ? "Saved TipsData.csv" : "Failed to save TipsData.csv";
	return static_cast<bool>(file);
}

///// ----- 画像 ----- /////

uint32_t Tips::GetTextureHandle(const std::string& image) {
	auto itr = textureHandles_.find(image);
	if (itr != textureHandles_.end()) {
		return itr->second;
	}

	// 存在しないパスを渡すと停止してしまうので、先に実ファイルを確認する。
	std::error_code error;
	if (!std::filesystem::is_regular_file(std::filesystem::path("Resources") / image, error)) {
		textureHandles_[image] = 0;
		return 0;
	}

	const uint32_t handle = TextureManager::Load(image);
	textureHandles_[image] = handle;
	return handle;
}

Model* Tips::GetModel(const std::string& image) {
	auto itr = models_.find(image);
	if (itr != models_.end()) {
		return itr->second.get();
	}

	// 背景と同じ「幅1・高さ1の板」モデルを使う。テクスチャは描画時に差し替える。
	std::unique_ptr<Model> model(Model::CreateFromOBJ("BackGround"));
	model->SetLightGroup(light_.get());
	Model* result = model.get();
	models_.emplace(image, std::move(model));
	return result;
}

void Tips::CollectImageFiles() {
	imageFiles_.clear();

	std::error_code error;
	const std::filesystem::path directory = std::filesystem::path("Resources") / kTipsImageDirectory;
	if (!std::filesystem::is_directory(directory, error)) {
		return;
	}

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, error)) {
		if (!entry.is_regular_file()) {
			continue;
		}
		std::string extension = entry.path().extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (extension != ".png") {
			continue;
		}
		imageFiles_.push_back(std::string(kTipsImageDirectory) + "/" + entry.path().filename().string());
	}

	std::sort(imageFiles_.begin(), imageFiles_.end());
}

///// ----- 描画 ----- /////

std::vector<Tips::Board>& Tips::CurrentBoards() { return boards_[stageNumber_]; }

void Tips::EnsureTransforms(size_t count) {
	while (transforms_.size() < count) {
		auto transform = std::make_unique<WorldTransform>();
		transform->Initialize();
		transforms_.push_back(std::move(transform));
	}
}

void Tips::Draw() {
	std::vector<Board>& boards = CurrentBoards();
	EnsureTransforms(boards.size());

	for (size_t i = 0; i < boards.size(); ++i) {
		const Board& board = boards[i];
		const uint32_t textureHandle = GetTextureHandle(board.image);
		if (textureHandle == 0) {
			continue;
		}

		WorldTransform& transform = *transforms_[i];
		transform.scale_ = {board.width, board.height, 1.0f};
		transform.rotation_.y = board.rotationDegreeY * std::numbers::pi_v<float> / 180.0f;
		transform.translation_ = board.position;
		UpdateWorldTransform(transform);

		// 画像ごとに用意した板モデルで描く
		GetModel(board.image)->Draw(transform, *camera_, textureHandle);
	}
}

///// ----- ImGui ----- /////
// NOTE: ImGuiの表示文字列は日本語だと文字化けするため、英語表記にしている
void Tips::ShowImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Stage Tips");

	std::vector<Board>& boards = CurrentBoards();
	// 削除などで枚数が減っても、選択番号が範囲外にならないようにする
	if (boards.empty()) {
		selectedIndex_ = 0;
	} else {
		selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(boards.size()) - 1);
	}

	ImGui::Text("Stage: %d | Boards: %d", stageNumber_, static_cast<int>(boards.size()));
	ImGui::TextWrapped("Boards live in world space, so they scroll with the stage. Put png files in Resources/images/tips.");
	ImGui::Separator();

	/// --- 看板の追加と削除 ---
	if (ImGui::Button("Add Board")) {
		Board board;
		// 追加直後に見つけやすいよう、カメラの正面あたりに出す
		board.position = {camera_->translation_.x, camera_->translation_.y, 0.0f};
		if (!imageFiles_.empty()) {
			board.image = imageFiles_.front();
		}
		boards.push_back(board);
		selectedIndex_ = static_cast<int>(boards.size()) - 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Selected") && !boards.empty()) {
		boards.erase(boards.begin() + selectedIndex_);
		selectedIndex_ = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Rescan Images")) {
		CollectImageFiles();
	}

	if (boards.empty()) {
		ImGui::TextWrapped("No board on this stage. Press Add Board.");
		ImGui::TextWrapped("%s", message_.c_str());
		ImGui::End();
		return;
	}

	ImGui::SliderInt("Board Index", &selectedIndex_, 0, static_cast<int>(boards.size()) - 1);
	selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(boards.size()) - 1);

	Board& board = boards[selectedIndex_];

	/// --- 使う画像を選ぶ ---
	ImGui::Text("Image: %s", board.image.c_str());
	if (!imageFiles_.empty()) {
		std::vector<const char*> items;
		items.reserve(imageFiles_.size());
		int current = 0;
		for (size_t i = 0; i < imageFiles_.size(); ++i) {
			items.push_back(imageFiles_[i].c_str());
			if (imageFiles_[i] == board.image) {
				current = static_cast<int>(i);
			}
		}
		if (ImGui::Combo("Select Image", &current, items.data(), static_cast<int>(items.size()))) {
			board.image = imageFiles_[current];
		}
	}

	ImGui::Separator();

	/// --- 位置と大きさ ---
	ImGui::DragFloat3("Position (x, y, z)", &board.position.x, 0.05f);
	ImGui::DragFloat("Width", &board.width, 0.05f, 0.05f, 100.0f);
	ImGui::DragFloat("Height", &board.height, 0.05f, 0.05f, 100.0f);
	ImGui::DragFloat("Rotation Y (deg)", &board.rotationDegreeY, 1.0f, -180.0f, 180.0f);
	ImGui::TextWrapped("Larger z puts the board behind blocks, smaller z puts it in front.");

	ImGui::Separator();

	/// --- 保存と読み直し ---
	if (ImGui::Button("Save TipsData.csv")) {
		SaveCsv();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload TipsData.csv")) {
		LoadCsv();
	}
	ImGui::TextWrapped("%s", message_.c_str());

	ImGui::End();
#endif
}
