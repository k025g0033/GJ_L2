#pragma once

#include "KamataEngine.h"
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ステージ内に置くTIPS看板。
// 背景と違って画面固定ではなく、ブロックと同じワールド座標に置くので、カメラが動けば一緒に流れる。
// どの画像をどこに置くかはResources/map/TipsData.csvで管理し、ImGuiから調整して保存できる。
class Tips {
public:
	// 看板1枚分の設定
	struct Board {
		// Resourcesフォルダからの相対パス
		std::string image = "images/tips/tips_rei.png";
		// ワールド座標。zを大きくすると奥、小さくすると手前に出る。
		KamataEngine::Vector3 position = {0.0f, 0.0f, 0.0f};
		// 板の大きさ。1.0でブロック1マス分。
		float width = 3.0f;
		float height = 1.6875f;
		// Y軸まわりの傾き（度）。0で正面向き。
		float rotationDegreeY = 0.0f;
	};

	void Initialize(KamataEngine::Camera* camera, int stageNumber);
	// 描画パスの開始・終了はGameScene側で行う。
	void Draw();
	void ShowImGui();

private:
	bool LoadCsv();
	bool SaveCsv();
	// 画像を読み込む。同じ画像は使い回す。
	uint32_t GetTextureHandle(const std::string& image);
	// 画像ごとの板モデルを取り出す。無ければ作る。
	KamataEngine::Model* GetModel(const std::string& image);
	// Resources/images/tips/ にあるpngを一覧する（ImGuiの画像選択に使う）
	void CollectImageFiles();
	// 現在ステージの看板一覧（無ければ空の配列を作る）
	std::vector<Board>& CurrentBoards();
	// 看板の枚数分だけWorldTransformを用意する
	void EnsureTransforms(size_t count);

	KamataEngine::Camera* camera_ = nullptr;
	int stageNumber_ = 1;
	// 全ステージ分を保持する。保存時にまとめて書き出すため。
	std::map<int, std::vector<Board>> boards_;
	// 画像ごとに板モデルを持つ。
	// KamataEngineはマテリアルの定数バッファ1つでテクスチャ番号を渡しているので、
	// 同じModelを使い回して1フレーム内に別々のテクスチャで何度も描くと、
	// 最後に指定したテクスチャで全部が描かれてしまう。画像ごとにModelを分けてこれを避ける。
	std::unordered_map<std::string, std::unique_ptr<KamataEngine::Model>> models_;
	// 描いた絵の色をそのまま出すため、板だけ環境光100%で描画する
	std::unique_ptr<KamataEngine::LightGroup> light_;
	// 看板ごとのワールド変換
	std::vector<std::unique_ptr<KamataEngine::WorldTransform>> transforms_;
	// 読み込み済みテクスチャ（パス → ハンドル）
	std::unordered_map<std::string, uint32_t> textureHandles_;
	// ImGuiで選択中の看板番号
	int selectedIndex_ = 0;
	// Resources/images/tips/ にある画像の一覧
	std::vector<std::string> imageFiles_;
	std::string message_;
};
