#include "CloneBase.h"
#include "MapChipField.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;


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

///// ----- ブロックとの当たり判定 ----- /////
bool CloneBase::IsCollidingWithBlock(const Vector3& position, MapChipField* mapChipField) const {
	// 自機のCornerPositionと同じ考え方で4隅を調べる
	Vector3 offsetTable[4] = {
	    {kWidth / 2.0f,  -kHeight / 2.0f, 0.0f}, // 右下
	    {-kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // 左下
	    {kWidth / 2.0f,  kHeight / 2.0f,  0.0f}, // 右上
	    {-kWidth / 2.0f, kHeight / 2.0f,  0.0f}, // 左上
	};

	for (const Vector3& offset : offsetTable) {
		MapChipField::IndexSet indexSet = mapChipField->GetMapChipIndexByPosition(position + offset);
		if (mapChipField->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex) == MapChipType::kBlock) {
			return true; // どれか1隅でもブロックに重なっていたら衝突とみなす
		}
	}

	return false;
}