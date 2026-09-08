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

Line3D::~Line3D() {
	delete lineModel_;
	delete connectedModel_;
}

void Line3D::Initialize() {
	// 飛ばす接続リンク線と、今つながっている先を示すリンク線。
	// どちらも1x1の板ポリゴン(.obj)に、それぞれの画像を貼ったもの。
	// ※2Dのスプライトではなく3Dモデルにすることで、自機と同じZ位置に置ける
	// 　（＝自機の手前ではなく、中心から線が出る）
	lineModel_ = Model::CreateFromOBJ("LinkLine", true);
	connectedModel_ = Model::CreateFromOBJ("NowLinkLine", true);

	lineColor_.Initialize();
	lineColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
	// 予測線は同じ画像を薄くして使う
	predictionColor_.Initialize();
	predictionColor_.SetColor({1.0f, 1.0f, 1.0f, 0.35f});
	connectedColor_.Initialize();
	connectedColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});

	for (WorldTransform& worldTransform : lineWorldTransforms_) {
		worldTransform.Initialize();
	}
	for (WorldTransform& worldTransform : predictionWorldTransforms_) {
		worldTransform.Initialize();
	}
	connectedWorldTransform_.Initialize();
}

void Line3D::Update(
	const Vector3& origin, const Camera& camera, MapChipField* mapChipField,
	const std::vector<MapChipField::Rect>& reflectingRects,
	const std::vector<MapChipField::Rect>& blockingRects, bool canFire, bool isClone) {
	if (!isClone && Input::GetInstance()->TriggerKey(DIK_Q)) {
		isPredictionVisible_ = !isPredictionVisible_;
	}

	Vector3 direction = GetMouseDirection(origin, camera);
	Path fullPath = CalculatePath(origin, direction, mapChipField, reflectingRects, blockingRects);
	if (isClone) {
		predictionPath_.segmentCount = 0;
	} else {
		predictionPath_ = fullPath;
		if (predictionPath_.segmentCount > 2) {
			predictionPath_.segmentCount = 2;
		}
	}
	if (!isClone && canFire && linePath_.segmentCount == 0 && Input::GetInstance()->IsTriggerMouse(0)) {
		linePath_ = fullPath;
		lineTravelDistance_ = 0.0f;
		isCloneLine_ = isClone;
	} else if (linePath_.segmentCount > 0) {
		lineTravelDistance_ += kLineSpeed;
		if (lineTravelDistance_ >= GetPathLength(linePath_)) {
			linePath_.segmentCount = 0;
			lineTravelDistance_ = 0.0f;
		}
	}
}

void Line3D::Draw(const Camera& camera) {
	// 板ポリゴンで描くので、3Dモデルの描画（Model::PreDraw〜PostDraw）の中で呼ぶこと

	// 今どれにつながっているかを示す線（つながっている間はずっと出す）
	// 変形アニメーションに合わせて濃さが変わるので、完全に透明な時は描かない
	if (isConnectedVisible_ && connectedModel_ != nullptr && connectedAlpha_ > 0.0f) {
		connectedColor_.SetColor({1.0f, 1.0f, 1.0f, connectedAlpha_});
		PlaceLineQuad(connectedWorldTransform_, connectedSegment_.start, connectedSegment_.end);
		connectedModel_->Draw(connectedWorldTransform_, camera, &connectedColor_);
	}

	// 飛んでいる接続リンク線
	DrawPath(linePath_, camera, lineModel_, &lineColor_, lineWorldTransforms_, lineTravelDistance_);

	// 予測線
	if (isPredictionVisible_) {
		DrawPath(
		    predictionPath_, camera, lineModel_, &predictionColor_, predictionWorldTransforms_,
		    std::numeric_limits<float>::max());
	}
}

///// ----- 今つながっている先を示す線 ----- /////
void Line3D::SetConnectedLine(const Vector3& from, const Vector3& to, float alpha) {
	connectedSegment_.start = from;
	connectedSegment_.end = to;
	connectedAlpha_ = (alpha < 0.0f) ? 0.0f : ((alpha > 1.0f) ? 1.0f : alpha);
	isConnectedVisible_ = true;
}

void Line3D::ClearConnectedLine() { isConnectedVisible_ = false; }

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

Line3D::Path Line3D::CalculatePath(
	const Vector3& origin, const Vector3& initialDirection, MapChipField* mapChipField,
	const std::vector<MapChipField::Rect>& reflectingRects,
	const std::vector<MapChipField::Rect>& blockingRects) const {
	Path path;
	Vector3 currentOrigin = origin;
	Vector3 direction = initialDirection;

	for (int reflectionCount = 0; reflectionCount <= 2; ++reflectionCount) {
		BlockHit nearestHit;
		float nearestBlockingDistance = std::numeric_limits<float>::max();

		// 閉じている扉は通常ブロックと同じ反射面として扱う。
		for (const MapChipField::Rect& rect : reflectingRects) {
			float distance = 0.0f;
			Vector3 normal{};
			if (RayAabb(currentOrigin, direction, rect, distance, normal) && distance < nearestHit.distance) {
				nearestHit.isHit = true;
				nearestHit.distance = distance;
				nearestHit.normal = normal;
			}
		}

		// レーザーは壁と違って反射せず、接触位置で接続線を止める。
		for (const MapChipField::Rect& rect : blockingRects) {
			float distance = 0.0f;
			Vector3 normal{};
			if (RayAabb(currentOrigin, direction, rect, distance, normal) && distance < nearestBlockingDistance) {
				nearestBlockingDistance = distance;
			}
		}

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

		const bool isBlocked = nearestBlockingDistance < nearestHit.distance && nearestBlockingDistance < kMaxTravelDistance;
		float segmentDistance = isBlocked ? nearestBlockingDistance : (nearestHit.isHit ? std::min(nearestHit.distance, kMaxTravelDistance) : kMaxTravelDistance);
		Vector3 end = currentOrigin + direction * segmentDistance;
		path.segments[path.segmentCount++] = {currentOrigin, end};
		if (isBlocked) {
			break;
		}
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
	const Path& path, const Camera& camera, Model* model, ObjectColor* color,
	std::array<WorldTransform, 3>& worldTransforms, float drawDistance) {
	if (model == nullptr) {
		return;
	}

	float remainingDistance = drawDistance;
	for (size_t i = 0; i < path.segmentCount; ++i) {
		Vector3 difference = path.segments[i].end - path.segments[i].start;
		float length = Length(difference);
		if (length <= 0.00001f || remainingDistance <= 0.0f) {
			continue;
		}

		// 発射直後は、進んだぶんだけ短く描く（線が伸びていくように見せるため）
		float visibleLength = std::min(length, remainingDistance);
		Vector3 direction = difference / length;
		Vector3 visibleEnd = path.segments[i].start + direction * visibleLength;

		PlaceLineQuad(worldTransforms[i], path.segments[i].start, visibleEnd);
		model->Draw(worldTransforms[i], camera, color);

		remainingDistance -= length;
	}
}

///// ----- 板ポリゴンで線を引く ----- /////
// 3D空間の2点を結ぶように、1x1の板ポリゴンを引き伸ばして置く。
// 板ポリゴンの中心＝2点の中点なので、板の両端（＝画像の端の真ん中）がちょうど2点に重なる。
// Zは2点のZをそのまま使う（＝自機と同じ奥行き）ので、自機の手前ではなく中心から線が出る。
void Line3D::PlaceLineQuad(WorldTransform& worldTransform, const Vector3& start, const Vector3& end) const {
	Vector3 difference = end - start;
	float length = Length(difference);

	worldTransform.translation_ = (start + end) * 0.5f;

	// 板ポリゴンは1x1なので、xに太さ・yに長さを入れるだけでそのまま線の形になる
	worldTransform.scale_ = {kLineThickness, length, 1.0f};

	// 画像は「縦向き」に描かれているので、板ポリゴンの長い方(+Y)が
	// 2点を結ぶ方向を向くように、90度ずらして回転させる
	worldTransform.rotation_ = {0.0f, 0.0f, std::atan2(difference.y, difference.x) - MathUtility::kPI * 0.5f};

	UpdateWorldTransform(worldTransform);
}

void Line3D::ResetLine() {
	linePath_.segmentCount = 0;
	lineTravelDistance_ = 0.0f;
	isCloneLine_ = false;
}
