#include "ThrowAimIndicator.h"
#include "math/MathUtility.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility; // Vector3のoperator+などがこの名前空間にあるため必要

///// ----- 初期化 ----- /////
void ThrowAimIndicator::Initialize(KamataEngine::Camera* camera) {
	camera_ = camera;

	// 三角形の画像（Resources/images/cursor.png）を読み込む
	triangleTextureHandle_ = TextureManager::Load("images/cursor.png");
	// アンカーポイントを中央にしておくことで、円周上の座標＝三角形の中心になる
	triangleSprite_ = Sprite::Create(triangleTextureHandle_, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 0.5f});
	triangleSprite_->SetSize({triangleSize_, triangleSize_});
}

///// ----- 更新処理 ----- /////
void ThrowAimIndicator::Update(const Vector3& playerPosition, const Vector3& mouseWorldPosition) {
	playerPosition_ = playerPosition;

	// 自機からマウスカーソルへ向かうベクトルを求める（Z成分は無視して2D平面で扱う）
	Vector3 toMouse = mouseWorldPosition - playerPosition_;
	toMouse.z = 0.0f;
	float distance = Length(toMouse);

	// カーソルが自機とほぼ重なっている場合は、直前の方向を維持する（ゼロ除算防止）
	if (distance > 0.0001f) {
		throwDirection_ = toMouse / distance;
	}

	// 円周上で、マウスカーソル方向にあたる位置（三角形を表示する場所）
	Vector3 trianglePosition = playerPosition_ + throwDirection_ * circleRadius_;

	// スクリーン座標へ変換し、三角形の位置を更新する
	Vector2 screenPosition = ConvertWorldToScreen(trianglePosition);
	triangleSprite_->SetPosition(screenPosition);

	// 三角形画像は「上向き」がデフォルトなので、上方向(0,1)から時計回りの角度を求めて向きを合わせる
	float rotation = std::atan2(throwDirection_.x, throwDirection_.y);
	triangleSprite_->SetRotation(rotation);

	triangleSprite_->SetSize({triangleSize_, triangleSize_});
}

///// ----- 描画処理 ----- /////
void ThrowAimIndicator::Draw() {
	if (camera_ == nullptr) {
		return;
	}

	/// --- 自機を中心とした円をワイヤーフレームで描画する ---
	PrimitiveDrawer* primitiveDrawer = PrimitiveDrawer::GetInstance();
	primitiveDrawer->SetCamera(camera_);

	const Vector4 color = {0.1f, 1.0f, 0.25f, 1.0f};
	for (uint32_t i = 0; i < kSegmentCount; ++i) {
		float angle0 = std::numbers::pi_v<float> * 2.0f * static_cast<float>(i) / static_cast<float>(kSegmentCount);
		float angle1 = std::numbers::pi_v<float> * 2.0f * static_cast<float>(i + 1) / static_cast<float>(kSegmentCount);

		Vector3 point0 = playerPosition_ + Vector3{std::cos(angle0) * circleRadius_, std::sin(angle0) * circleRadius_, 0.0f};
		Vector3 point1 = playerPosition_ + Vector3{std::cos(angle1) * circleRadius_, std::sin(angle1) * circleRadius_, 0.0f};
		primitiveDrawer->DrawLine3d(point0, point1, color);
	}

	/// --- 円周上の三角形（画像）を描画する ---
	Sprite::PreDraw();
	triangleSprite_->Draw();
	Sprite::PostDraw();
}

///// ----- ワールド座標からスクリーン座標への変換 ----- /////
KamataEngine::Vector2 ThrowAimIndicator::ConvertWorldToScreen(const KamataEngine::Vector3& worldPosition) const {
	if (camera_ == nullptr) {
		return {};
	}

	RECT clientRect = {};
	GetClientRect(WinApp::GetInstance()->GetHwnd(), &clientRect);
	float clientWidth = static_cast<float>((std::max)(clientRect.right - clientRect.left, 1L));
	float clientHeight = static_cast<float>((std::max)(clientRect.bottom - clientRect.top, 1L));

	Matrix4x4 matViewProjection = MathUtility::operator*(camera_->matView, camera_->matProjection);
	Vector3 ndc = MathUtility::TransformCoord(worldPosition, matViewProjection);

	float screenX = (ndc.x + 1.0f) * 0.5f * clientWidth;
	float screenY = (1.0f - ndc.y) * 0.5f * clientHeight;
	return {screenX, screenY};
}
