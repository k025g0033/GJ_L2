#pragma once
#include "KamataEngine.h"

enum class MapChipType {
	kBlank, // 空白
	kBlock, // ブロック
	kPlayer, // プレイヤー
	kLazer,  // レーザー
	kCloneBase, // クローンの素
	kWater,     // 水
	kPushPlate, // 感圧板
	kDoor, // 扉
	kGoal, // ゴール
};

// 1マス分のデータ
struct MapChipDataUnit {
	MapChipType type = MapChipType::kBlank; // マップチップの種別
	uint8_t subID = 0;                      // 種類ごとのサブID
	uint8_t requiredActorCount = 1;         // 感圧板を作動させる必要人数
};

struct MapChipData {
	std::vector<std::vector<MapChipDataUnit>> data;
};

class MapChipField {
public:

	// マップチップCSVの文字番号
	enum MapChipCsvIndex {
		kChipType = 0,  // 種別
		kChipSubID = 1, // サブID
		kChipRequiredCount = 2, // 感圧板の必要人数
	};

	struct IndexSet {
		uint32_t xIndex;
		uint32_t yIndex;
	};

	// 範囲矩形
	struct Rect {
		float left;
		float right;
		float bottom;
		float top;
	};

	// １ブロックのサイズ
	static inline const float kBlockWidth = 1.0f;
	static inline const float kBlockHeight = 1.0f;

	// ブロックの個数
	static inline const uint32_t kNumBlockVertical = 20;
	static inline const uint32_t kNumBlockHorizontal = 100;

	MapChipData mapChipData_;

	// リセット
	void ResetMapChipData();
	// 読み込み
	void LoadMapChipCsv(const std::string& filePath);
	// マップチップ種別の取得
	MapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex);
	// マップチップ座標の取得
	KamataEngine::Vector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex);

	IndexSet GetMapChipIndexByPosition(const KamataEngine::Vector3& position);

	Rect GetRectByIndex(uint32_t xIndex, uint32_t yIndex);

	uint8_t GetMapChipSubIDByIndex(uint32_t xIndex, uint32_t yIndex);
	uint8_t GetRequiredActorCountByIndex(uint32_t xIndex, uint32_t yIndex);
};
