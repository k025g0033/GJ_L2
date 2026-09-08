#include "ThrowAimIndicator.h"
#include "WorldTransformConfig.h"
#include "math/MathUtility.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility; // Vector3のoperator+などがこの名前空間にあるため必要

ThrowAimIndicator::~ThrowAimIndicator() { delete triangleModel_; }

///// ----- 初期化 ----- /////
void ThrowAimIndicator::Initialize(KamataEngine::Camera* camera) {
	camera_ = camera;

	// 三角形の板ポリゴン（Resources/ThrowAim。cursor.pngを貼ってある）
	// ※以前はスプライト（2D）で、ワールド座標をスクリーン座標に変換して置いていたが、
	// 　円のほうはワイヤーフレーム（3D）で描いているため、2つの座標系がずれて
	// 　三角形が円周から外れて見えていた。3Dモデルにして同じ空間で扱うことで必ず円周に乗る。
	triangleModel_ = Model::CreateFromOBJ("ThrowAim", true);
	triangleWorldTransform_.Initialize();
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

	// 円周上で、マウスカーソル方向にあたる位置（三角形を表示する場所）。
	// 円と同じワールド座標なので、必ず円周にぴったり乗る。
	triangleWorldTransform_.translation_ = playerPosition_ + throwDirection_ * circleRadius_;

	triangleWorldTransform_.scale_ = {triangleSize_, triangleSize_, 1.0f};

	// 三角形の画像は「上向き」なので、板ポリゴンの上方向(+Y)が投げる方向を向くように90度ずらす
	triangleWorldTransform_.rotation_ = {
	    0.0f, 0.0f, std::atan2(throwDirection_.y, throwDirection_.x) + std::numbers::pi_v<float> * 0.5f};

	UpdateWorldTransform(triangleWorldTransform_);
}

///// ----- 描画処理（三角形。3Dモデルの描画パスの中で呼ぶ） ----- /////
void ThrowAimIndicator::DrawModel() {
	if (camera_ == nullptr || triangleModel_ == nullptr) {
		return;
	}

	triangleModel_->Draw(triangleWorldTransform_, *camera_);
}

///// ----- 描画処理（円。3Dモデルの描画パスの外で呼ぶ） ----- /////
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
}
