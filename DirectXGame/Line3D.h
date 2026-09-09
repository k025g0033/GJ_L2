#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "KamataEngine.h"
#include "MapChipField.h"
#include <array>
#include <vector>

// ブロックで最大2回反射する3Dの線
class Line3D {
public:
	~Line3D();

	void Initialize();
	void Update(
	    const KamataEngine::Vector3& origin, const KamataEngine::Camera& camera, MapChipField* mapChipField,
	    const std::vector<MapChipField::Rect>& reflectingRects,
	    const std::vector<MapChipField::Rect>& blockingRects, bool canFire, bool isClone);
	// 描画（板ポリゴンで描くので、Model::PreDraw〜PostDrawの中で呼ぶこと）
	void Draw(const KamataEngine::Camera& camera);

	///// ----- 今つながっている先を示す線 ----- /////
	// 自機と、リンク中のクローンの間に出しっぱなしにする線。
	// 飛ばす接続線とは別の画像（nowLinkLine.png）で描く。
	// alphaは濃さ（0.0〜1.0）。変形アニメーションに合わせてフェードイン／フェードアウトさせるために使う。
	void SetConnectedLine(const KamataEngine::Vector3& from, const KamataEngine::Vector3& to, float alpha = 1.0f);
	void ClearConnectedLine();
	bool IsActive() const { return linePath_.segmentCount > 0; }
	bool IsCloneLine() const { return isCloneLine_; }
	bool IsPredictionVisible() const { return isPredictionVisible_; }
	void SetPredictionVisible(bool isVisible) { isPredictionVisible_ = isVisible; }
	bool IsTouchingSphere(const KamataEngine::Vector3& center, float radius) const;

	// 接続解除
	void ResetLine();

private:
	struct Segment {
		KamataEngine::Vector3 start{};
		KamataEngine::Vector3 end{};
	};

	struct Path {
		std::array<Segment, 3> segments{};
		size_t segmentCount = 0;
	};

	KamataEngine::Vector3 GetMouseDirection(const KamataEngine::Vector3& origin, const KamataEngine::Camera& camera) const;
	Path CalculatePath(
	    const KamataEngine::Vector3& origin, const KamataEngine::Vector3& direction, MapChipField* mapChipField,
	    const std::vector<MapChipField::Rect>& reflectingRects,
	    const std::vector<MapChipField::Rect>& blockingRects) const;
	float GetPathLength(const Path& path) const;
	void DrawPath(
	    const Path& path, const KamataEngine::Camera& camera, KamataEngine::Model* model,
	    KamataEngine::ObjectColor* color, std::array<KamataEngine::WorldTransform, 3>& worldTransforms,
	    float drawDistance);

	///// ----- 板ポリゴンで線を引くための処理 ----- /////
	// 3D空間の2点を結ぶように、線の板ポリゴンを1枚ぶん配置する。
	// Zは2点のZをそのまま使うので、自機と同じ奥行き（＝中心）から線が出る。
	void PlaceLineQuad(
	    KamataEngine::WorldTransform& worldTransform, const KamataEngine::Vector3& start,
	    const KamataEngine::Vector3& end) const;

	// 飛ばす接続リンク線（Resources/LinkLine。linkLine.pngを貼った1x1の板ポリゴン）
	KamataEngine::Model* lineModel_ = nullptr;
	// 今つながっている先を示すリンク線（Resources/NowLinkLine。nowLinkLine.pngを貼った板ポリゴン）
	KamataEngine::Model* connectedModel_ = nullptr;

	KamataEngine::ObjectColor lineColor_;
	KamataEngine::ObjectColor predictionColor_;
	KamataEngine::ObjectColor connectedColor_;

	// 反射で最大3本に分かれるので、線1本につきワールド変換を3つ持つ
	std::array<KamataEngine::WorldTransform, 3> lineWorldTransforms_{};
	std::array<KamataEngine::WorldTransform, 3> predictionWorldTransforms_{};
	// 今つながっている先を示す線は、自機とクローンを結ぶ1本だけ
	KamataEngine::WorldTransform connectedWorldTransform_{};
	Segment connectedSegment_{};
	bool isConnectedVisible_ = false;
	// 今つながっている先を示す線の濃さ（0.0で完全に透明、1.0で完全に表示）
	float connectedAlpha_ = 1.0f;

	Path linePath_{};
	Path predictionPath_{};
	bool isPredictionVisible_ = true;
	bool isCloneLine_ = false;
	float lineTravelDistance_ = 0.0f;
	// 線の太さ（ワールド単位）。自機の当たり判定の横幅が0.8なので、それを目安に決める。
	// 太さ(x)はこの値のまま固定で、長さ(y)だけ2点の距離まで伸ばす。
	static inline const float kLineThickness = 0.2f;
	static inline const float kLineSpeed = 0.5f;
	static inline const float kMaxTravelDistance = 30.0f;
};
