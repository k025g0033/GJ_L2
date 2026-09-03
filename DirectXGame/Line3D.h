#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "KamataEngine.h"
#include <array>

class MapChipField;

// ブロックで最大2回反射する3Dの線
class Line3D {
public:
	~Line3D();

	void Initialize();
	void Update(
	    const KamataEngine::Vector3& origin, const KamataEngine::Camera& camera, MapChipField* mapChipField,
	    bool canFire);
	void Draw(const KamataEngine::Camera& camera);
	bool IsActive() const { return linePath_.segmentCount > 0; }

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
	Path CalculatePath(const KamataEngine::Vector3& origin, const KamataEngine::Vector3& direction, MapChipField* mapChipField) const;
	float GetPathLength(const Path& path) const;
	void DrawPath(
	    const Path& path, const KamataEngine::Camera& camera, KamataEngine::ObjectColor* color,
	    std::array<KamataEngine::WorldTransform, 3>& worldTransforms, float drawDistance);

	KamataEngine::Model* model_ = nullptr;
	KamataEngine::ObjectColor lineColor_;
	KamataEngine::ObjectColor predictionColor_;
	std::array<KamataEngine::WorldTransform, 3> lineWorldTransforms_{};
	std::array<KamataEngine::WorldTransform, 3> predictionWorldTransforms_{};
	Path linePath_{};
	Path predictionPath_{};
	bool isPredictionVisible_ = true;
	float lineTravelDistance_ = 0.0f;
	static inline const float kLineSpeed = 0.5f;
};
