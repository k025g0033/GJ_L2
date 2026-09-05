#include "CloneBase.h"
#include "MapChipField.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

CloneBase::~CloneBase() { delete player_; }

void CloneBase::Initialize(
	Model* modelBase, Model* modelClone, Camera* camera, MapChipField* mapChipField, const Vector3& position) {
	modelBase_ = modelBase;
	modelClone_ = modelClone;
	camera_ = camera;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};
	initialPosition_ = position; // 初期位置を保存

	state_ = State::kBase;

	player_ = new Player();
	player_->Initialize(modelClone_, camera_, position);
	player_->SetMapChipField(mapChipField);
}

void CloneBase::Update(bool isControlled, const std::vector<MapChipField::Rect>& obstacleRects) {
	if (state_ == State::kTransformed) {
		player_->Update(isControlled, obstacleRects);

			if (player_->IsInWater()) {
			// プレイヤー型クローンを初期位置へ戻す
			player_->Respawn(initialPosition_);

			// 球体のクローンの素も初期位置へ戻す
			worldTransform_.translation_ = initialPosition_;
			worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};

			// 見た目をプレイヤー型から球体へ戻す
			state_ = State::kBase;

			// GameSceneへ消滅を通知
			wasDestroyedByWater_ = true;

			UpdateWorldTransform(worldTransform_);
		}

		return;
	}

	// 変形状態に応じてスケールを切り替える
	// （素：球体を1マスに収めるスケール／変形後：自機と同じ等身大スケール）
	float scale = (state_ == State::kTransformed) ? 1.0f : kBaseScale;
	worldTransform_.scale_ = {scale, scale, scale};

	UpdateWorldTransform(worldTransform_);
}

void CloneBase::Draw() {
	if (state_ == State::kTransformed) {
		player_->Draw();
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

bool CloneBase::ConsumeWaterDestroyed() {
	if (!wasDestroyedByWater_) {
		return false;
	}

	wasDestroyedByWater_ = false;
	return true;
}