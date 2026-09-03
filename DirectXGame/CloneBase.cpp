#include "CloneBase.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;

void CloneBase::Initialize(Model* modelBase, Model* modelClone, Camera* camera, const Vector3& position) {
	modelBase_ = modelBase;
	modelClone_ = modelClone;
	camera_ = camera;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};

	state_ = State::kBase;
}

void CloneBase::Update() {
	// 変形状態に応じてスケールを切り替える
	// （素：球体を1マスに収めるスケール／変形後：自機と同じ等身大スケール）
	float scale = (state_ == State::kTransformed) ? 1.0f : kBaseScale;
	worldTransform_.scale_ = {scale, scale, scale};

	UpdateWorldTransform(worldTransform_);
}

void CloneBase::Draw() {
	if (state_ == State::kTransformed) {
		// 線がつながった後は自機と同じモデルで描画する
		modelClone_->Draw(worldTransform_, *camera_);
	} else {
		// 素の状態は球体で描画する
		modelBase_->Draw(worldTransform_, *camera_);
	}
}
