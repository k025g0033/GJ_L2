#include "Lazer.h"
#include "WorldTransformConfig.h"
#include <cassert>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

void Lazer::Initialize(Model* model, Camera* camera, const Vector3& start, const Vector3& end) {
	assert(model);
	assert(camera);

	model_ = model;
	camera_ = camera;

	worldTransform_.Initialize();

	// Lazer.objはY軸方向の円柱で、原点がモデル中央ではない
	constexpr float kModelLength = 8.0f;
	constexpr float kModelCenterY = 4.0f;
	constexpr float kRadiusScale = 0.1f;

	Vector3 center = {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f, 0.0f};

	if (start.y == end.y) {
		// 横レーザー
		// 両端のLマス全体まで届くように1マス分を加える
		float length = std::abs(end.x - start.x) + 1.0f;
		float lengthScale = length / kModelLength;
		worldTransform_.scale_ = {kRadiusScale, lengthScale, kRadiusScale};
		worldTransform_.rotation_.z = std::numbers::pi_v<float> / 2.0f;
		// +Y軸を+90度回転すると-X軸方向になるため、モデル中心のずれを補正する
		worldTransform_.translation_ = {center.x + kModelCenterY * lengthScale, center.y, center.z};

	} else if (start.x == end.x) {
		// 縦レーザー
		// 両端のLマス全体まで届くように1マス分を加える
		float length = std::abs(end.y - start.y) + 1.0f;
		float lengthScale = length / kModelLength;
		worldTransform_.scale_ = {kRadiusScale, lengthScale, kRadiusScale};
		worldTransform_.translation_ = {center.x, center.y - kModelCenterY * lengthScale, center.z};

	} else {
		// L0が斜めに置かれている
		assert(false && "L0 must be placed horizontally or vertically");
	}

	UpdateWorldTransform(worldTransform_);
}

void Lazer::Update() { UpdateWorldTransform(worldTransform_); }

void Lazer::Draw() { model_->Draw(worldTransform_, *camera_); }
