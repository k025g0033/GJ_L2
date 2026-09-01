#define NOMINMAX
#include "CameraController.h"
#include "KamataEngine.h"
#include "Player.h"
#include "math/MathUtility.h"
#include <algorithm>


using namespace KamataEngine::MathUtility;

KamataEngine::Vector3 Lerp(const KamataEngine::Vector3& start, const KamataEngine::Vector3& end, float t) {
	KamataEngine::Vector3 result;
	result.x = start.x + (end.x - start.x) * t;
	result.y = start.y + (end.y - start.y) * t;
	result.z = start.z + (end.z - start.z) * t;
	return result;
}

void CameraController::Initialize(KamataEngine::Camera* camera) {
	// カメラの初期化
	camera_ = camera;
}

void CameraController::Update() {
	// 追従対象のワールドトランスフォームを参照
	const KamataEngine::WorldTransform& targetWorldTransform = target_->GetWorldTransform();
	// 追従対象とオフセットからカメラの座標を計算
	KamataEngine::Vector3 targetVelocity = target_->GetVelocity();
	targetPos_ = targetWorldTransform.translation_ + targetOffset_ + targetVelocity * kVelocityBias;

	// 座標補間によりゆったり追従
	camera_->translation_ = Lerp(camera_->translation_, targetPos_, kInterpolationRate);

	// 追従対象が画面外に出ないように補正
	camera_->translation_.x = std::max(camera_->translation_.x, targetWorldTransform.translation_.x + margin.left);
	camera_->translation_.x = std::min(camera_->translation_.x, targetWorldTransform.translation_.x + margin.right);
	camera_->translation_.y = std::max(camera_->translation_.y, targetWorldTransform.translation_.y + margin.bottom);
	camera_->translation_.y = std::min(camera_->translation_.y, targetWorldTransform.translation_.y + margin.top);

	// 移動範囲制限
	camera_->translation_.x = std::max(camera_->translation_.x, movableArea_.left);
	camera_->translation_.x = std::min(camera_->translation_.x, movableArea_.right);
	camera_->translation_.y = std::max(camera_->translation_.y, movableArea_.bottom);
	camera_->translation_.y = std::min(camera_->translation_.y, movableArea_.top);

	// 行列を更新する
	camera_->UpdateMatrix();
}

void CameraController::Reset() {
	// 追従対象のワールドトランスフォームを参照
	const KamataEngine::WorldTransform& targetWorldTransform = target_->GetWorldTransform();
	// 追従対象とオフセットからカメラの座標を計算
	camera_->translation_ = targetWorldTransform.translation_ + targetOffset_;
}