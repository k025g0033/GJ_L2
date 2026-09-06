#include "MouseCursor.h"
#include "math/MathUtility.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility; // Vector3のoperator+などがこの名前空間にあるため必要

///// ----- 初期化 ----- /////
void MouseCursor::Initialize(KamataEngine::Camera* camera) {
	camera_ = camera;

	// 初回から実際のマウス位置へ表示できるように更新する
	Update();
}

///// ----- 更新処理 ----- /////
void MouseCursor::Update() {
	Input* input = Input::GetInstance();

	// エンジンが管理しているウィンドウ座標を取得する
	screenPosition_ = input->GetMousePosition();
	worldPosition_ = ConvertScreenToWorld(screenPosition_);

	// 左ボタンを押した瞬間だけクリックとして扱う
	isClicked_ = input->IsTriggerMouse(0);
}

///// ----- 描画処理 ----- /////
void MouseCursor::Draw() {
	if (camera_ == nullptr) {
		return;
	}

	// テクスチャを使わず、短い3D線分をつないで緑色の円を作る
	PrimitiveDrawer* primitiveDrawer = PrimitiveDrawer::GetInstance();
	primitiveDrawer->SetCamera(camera_);

	const Vector4 color = {0.1f, 1.0f, 0.25f, 1.0f};
	for (uint32_t i = 0; i < kSegmentCount; ++i) {
		float angle0 = std::numbers::pi_v<float> * 2.0f * static_cast<float>(i) / static_cast<float>(kSegmentCount);
		float angle1 = std::numbers::pi_v<float> * 2.0f * static_cast<float>(i + 1) / static_cast<float>(kSegmentCount);

		Vector3 point0 = worldPosition_ + Vector3{std::cos(angle0) * kRadius, std::sin(angle0) * kRadius, 0.0f};
		Vector3 point1 = worldPosition_ + Vector3{std::cos(angle1) * kRadius, std::sin(angle1) * kRadius, 0.0f};
		primitiveDrawer->DrawLine3d(point0, point1, color);
	}
}

///// ----- ウィンドウ座標からワールド座標への変換 ----- /////
KamataEngine::Vector3 MouseCursor::ConvertScreenToWorld(const KamataEngine::Vector2& screenPosition) const {
	if (camera_ == nullptr) {
		return {};
	}

	RECT clientRect = {};
	GetClientRect(WinApp::GetInstance()->GetHwnd(), &clientRect);
	float clientWidth = static_cast<float>((std::max)(clientRect.right - clientRect.left, 1L));
	float clientHeight = static_cast<float>((std::max)(clientRect.bottom - clientRect.top, 1L));

	float ndcX = screenPosition.x / clientWidth * 2.0f - 1.0f;
	float ndcY = 1.0f - screenPosition.y / clientHeight * 2.0f;

	Matrix4x4 matViewProjection = MathUtility::operator*(camera_->matView, camera_->matProjection);
	Matrix4x4 matInverseViewProjection = MathUtility::Inverse(matViewProjection);
	Vector3 nearPosition = MathUtility::TransformCoord({ndcX, ndcY, 0.0f}, matInverseViewProjection);
	Vector3 farPosition = MathUtility::TransformCoord({ndcX, ndcY, 1.0f}, matInverseViewProjection);
	Vector3 direction = {farPosition.x - nearPosition.x, farPosition.y - nearPosition.y, farPosition.z - nearPosition.z};

	if (std::abs(direction.z) < 0.0001f) {
		return {nearPosition.x, nearPosition.y, kCursorPlaneZ};
	}
	float t = (kCursorPlaneZ - nearPosition.z) / direction.z;
	return {nearPosition.x + direction.x * t, nearPosition.y + direction.y * t, nearPosition.z + direction.z * t};
}