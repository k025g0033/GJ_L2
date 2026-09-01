#include "WorldTransformConfig.h"

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

void UpdateWorldTransform(WorldTransform& worldTransform) {

	Matrix4x4 matScale = MathUtility::MakeScaleMatrix(worldTransform.scale_);

	Matrix4x4 matRotX = MathUtility::MakeRotateXMatrix(worldTransform.rotation_.x);
	Matrix4x4 matRotY = MathUtility::MakeRotateYMatrix(worldTransform.rotation_.y);
	Matrix4x4 matRotZ = MathUtility::MakeRotateZMatrix(worldTransform.rotation_.z);
	Matrix4x4 matRot = matRotX * matRotY * matRotZ;

	Matrix4x4 matTrans = MathUtility::MakeTranslateMatrix(worldTransform.translation_);

	// 2. 行列を合成 (Scale * Rotation * Translation)
	worldTransform.matWorld_ = matScale * matRot * matTrans;

	// 3. 定数バッファへ転送
	worldTransform.TransferMatrix();
}