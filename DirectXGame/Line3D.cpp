#include "Line3D.h"
#include "MapChipField.h"
#include "WorldTransformConfig.h"
#include "math/MathUtility.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

namespace {

constexpr float kMaxDistance = 100.0f;
constexpr float kHitEpsilon = 0.01f;
constexpr float kLineRadiusScale = 0.15f;

struct BlockHit {
	bool isHit = false;
	float distance = std::numeric_limits<float>::max();
	Vector3 normal{};
};

bool RayAabb(
    const Vector3& origin, const Vector3& direction, const MapChipField::Rect& rect, float& distance, Vector3& normal) {
	float nearDistance = 0.0f;
	float farDistance = kMaxDistance;
	Vector3 nearNormal{};

	auto TestAxis = [&](float originValue, float directionValue, float minimum, float maximum, const Vector3& minimumNormal,
	                    const Vector3& maximumNormal) {
		if (std::abs(directionValue) < 0.00001f) {
			return originValue >= minimum && originValue <= maximum;
		}

		float first = (minimum - originValue) / directionValue;
		float second = (maximum - originValue) / directionValue;
		Vector3 firstNormal = minimumNormal;
		Vector3 secondNormal = maximumNormal;

		if (first > second) {
			std::swap(first, second);
			std::swap(firstNormal, secondNormal);
		}

		if (first > nearDistance) {
			nearDistance = first;
			nearNormal = firstNormal;
		}

		farDistance = std::min(farDistance, second);
		return nearDistance <= farDistance;
	};

	if (!TestAxis(origin.x, direction.x, rect.left, rect.right, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f})) {
		return false;
	}
	if (!TestAxis(origin.y, direction.y, rect.bottom, rect.top, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f})) {
		return false;
	}
	if (nearDistance <= kHitEpsilon) {
		return false;
	}

	distance = nearDistance;
	normal = nearNormal;
	return true;
}

} // namespace

Line3D::~Line3D() { delete model_; }

void Line3D::Initialize() {
	model_ = Model::CreateFromOBJ("Line", true);

	lineColor_.Initialize();
	lineColor_.SetColor({0.2f, 0.8f, 1.0f, 1.0f});
	predictionColor_.Initialize();
	predictionColor_.SetColor({0.2f, 0.8f, 1.0f, 0.35f});

	for (WorldTransform& worldTransform : lineWorldTransforms_) {
		worldTransform.Initialize();
	}
	for (WorldTransform& worldTransform : predictionWorldTransforms_) {
		worldTransform.Initialize();
	}
}

void Line3D::Update(
	const Vector3& origin, const Camera& camera, MapChipField* mapChipField, bool canFire, bool isClone) {
	if (Input::GetInstance()->TriggerKey(DIK_Q)) {
		isPredictionVisible_ = !isPredictionVisible_;
	}

	Vector3 direction = GetMouseDirection(origin, camera);
	Path fullPath = CalculatePath(origin, direction, mapChipField);
	predictionPath_ = fullPath;
	if (predictionPath_.segmentCount > 2) {
		predictionPath_.segmentCount = 2;
	}
	if (canFire && linePath_.segmentCount == 0 && Input::GetInstance()->IsTriggerMouse(0)) {
		linePath_ = fullPath;
		lineTravelDistance_ = 0.0f;
		isCloneLine_ = isClone;
		lineColor_.SetColor(isClone ? Vector4{1.0f, 1.0f, 0.0f, 1.0f} : Vector4{0.2f, 0.8f, 1.0f, 1.0f});
	} else if (linePath_.segmentCount > 0) {
		lineTravelDistance_ += kLineSpeed;
		if (lineTravelDistance_ >= GetPathLength(linePath_)) {
			linePath_.segmentCount = 0;
			lineTravelDistance_ = 0.0f;
		}
	}
}

void Line3D::Draw(const Camera& camera) {
	DrawPath(linePath_, camera, &lineColor_, lineWorldTransforms_, lineTravelDistance_);
	if (isPredictionVisible_) {
		DrawPath(predictionPath_, camera, &predictionColor_, predictionWorldTransforms_, std::numeric_limits<float>::max());
	}
}

bool Line3D::IsTouchingSphere(const Vector3& center, float radius) const {
	float remainingDistance = lineTravelDistance_;
	for (size_t i = 0; i < linePath_.segmentCount && remainingDistance > 0.0f; ++i) {
		Vector3 difference = linePath_.segments[i].end - linePath_.segments[i].start;
		float length = Length(difference);
		if (length <= 0.00001f) {
			continue;
		}

		float visibleLength = std::min(length, remainingDistance);
		Vector3 direction = difference / length;
		Vector3 toCenter = center - linePath_.segments[i].start;
		float nearestDistance = std::clamp(Dot(toCenter, direction), 0.0f, visibleLength);
		Vector3 nearestPoint = linePath_.segments[i].start + direction * nearestDistance;
		if (Length(center - nearestPoint) <= radius) {
			return true;
		}

		remainingDistance -= length;
	}
	return false;
}

Vector3 Line3D::GetMouseDirection(const Vector3& origin, const Camera& camera) const {
	const Vector2& mouse = Input::GetInstance()->GetMousePosition();
	Vector3 nearPosition = {mouse.x / 1280.0f * 2.0f - 1.0f, 1.0f - mouse.y / 720.0f * 2.0f, 0.0f};
	Vector3 farPosition = {nearPosition.x, nearPosition.y, 1.0f};
	Matrix4x4 inverseViewProjection = Inverse(camera.matView * camera.matProjection);
	nearPosition = TransformCoord(nearPosition, inverseViewProjection);
	farPosition = TransformCoord(farPosition, inverseViewProjection);
	Vector3 mouseRay = farPosition - nearPosition;
	Vector3 target = farPosition;

	if (std::abs(mouseRay.z) > 0.00001f) {
		target = nearPosition + mouseRay * ((origin.z - nearPosition.z) / mouseRay.z);
	}

	Vector3 direction = {target.x - origin.x, target.y - origin.y, 0.0f};
	if (Length(direction) < 0.00001f) {
		return {1.0f, 0.0f, 0.0f};
	}
	Normalize(direction);
	return direction;
}

Line3D::Path Line3D::CalculatePath(const Vector3& origin, const Vector3& initialDirection, MapChipField* mapChipField) const {
	Path path;
	Vector3 currentOrigin = origin;
	Vector3 direction = initialDirection;

	for (int reflectionCount = 0; reflectionCount <= 2; ++reflectionCount) {
		BlockHit nearestHit;

		for (uint32_t y = 0; y < MapChipField::kNumBlockVertical; ++y) {
			for (uint32_t x = 0; x < MapChipField::kNumBlockHorizontal; ++x) {
				if (mapChipField->GetMapChipTypeByIndex(x, y) != MapChipType::kBlock) {
					continue;
				}

				MapChipField::Rect rect = mapChipField->GetRectByIndex(x, y);
				float distance = 0.0f;
				Vector3 normal{};
				if (RayAabb(currentOrigin, direction, rect, distance, normal) && distance < nearestHit.distance) {
					nearestHit.isHit = true;
					nearestHit.distance = distance;
					nearestHit.normal = normal;
				}
			}
		}

		float segmentDistance = nearestHit.isHit ? std::min(nearestHit.distance, kMaxTravelDistance) : kMaxTravelDistance;
		Vector3 end = currentOrigin + direction * segmentDistance;
		path.segments[path.segmentCount++] = {currentOrigin, end};
		if (!nearestHit.isHit || nearestHit.distance >= kMaxTravelDistance || reflectionCount == 2) {
			break;
		}

		direction = direction - nearestHit.normal * (2.0f * Dot(direction, nearestHit.normal));
		currentOrigin = end + direction * kHitEpsilon;
	}
	return path;
}

float Line3D::GetPathLength(const Path& path) const {
	float totalLength = 0.0f;
	for (size_t i = 0; i < path.segmentCount; ++i) {
		totalLength += Length(path.segments[i].end - path.segments[i].start);
	}
	return totalLength;
}

void Line3D::DrawPath(
	const Path& path, const Camera& camera, ObjectColor* color, std::array<WorldTransform, 3>& worldTransforms,
	float drawDistance) {
	float remainingDistance = drawDistance;
	for (size_t i = 0; i < path.segmentCount; ++i) {
		Vector3 difference = path.segments[i].end - path.segments[i].start;
		float length = Length(difference);
		if (length <= 0.00001f || remainingDistance <= 0.0f) {
			continue;
		}

		float visibleLength = std::min(length, remainingDistance);
		Vector3 direction = difference / length;
		Vector3 visibleEnd = path.segments[i].start + direction * visibleLength;

		WorldTransform& worldTransform = worldTransforms[i];
		worldTransform.translation_ = (path.segments[i].start + visibleEnd) * 0.5f;
		worldTransform.translation_.z -= 0.2f;
		worldTransform.scale_ = {kLineRadiusScale, kLineRadiusScale, visibleLength * 0.5f};
		worldTransform.rotation_ = {
		    -MathUtility::kPI * 0.5f, 0.0f, std::atan2(difference.y, difference.x) - MathUtility::kPI * 0.5f};
		UpdateWorldTransform(worldTransform);
		model_->Draw(worldTransform, camera, color);
		remainingDistance -= length;
	}
}

void Line3D::ResetLine() {
	linePath_.segmentCount = 0;
	lineTravelDistance_ = 0.0f;
	isCloneLine_ = false;
}
