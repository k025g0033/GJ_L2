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
	mapChipField_ = mapChipField;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};
	initialPosition_ = position; // 初期位置を保存

	state_ = State::kBase;

	player_ = new Player();
	player_->Initialize(modelClone_, camera_, position);
	player_->SetMapChipField(mapChipField);
}

void CloneBase::Update(bool isControlled, const std::vector<MapChipField::Rect>& obstacleRects, const MapChipField::Rect& playerRect) {
	if (state_ == State::kTransformed) {
		player_->Update(isControlled, obstacleRects);

		if (player_->IsInWater()) {
			player_->Respawn(initialPosition_);
			worldTransform_.translation_ = initialPosition_;
			worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};
			state_ = State::kBase;
			wasDestroyedByWater_ = true;
			UpdateWorldTransform(worldTransform_);
		}
		return;
	}

	// 投げられている（＝重力が働いている）間は、毎フレーム着地判定をやり直す。
	// こうしておくと、自機の上に乗った後で自機が動いた時に、支えがなくなって自然に落下を再開する。
	if (isThrown_) {
		UpdateThrowPhysics(playerRect);
	}

	// 変形状態に応じてスケールを切り替える
	float scale = (state_ == State::kTransformed) ? 1.0f : kBaseScale;
	worldTransform_.scale_ = {scale, scale, scale};

	UpdateWorldTransform(worldTransform_);
}

///// ----- 投げられて飛んでいる間の物理更新 ----- /////
void CloneBase::UpdateThrowPhysics(const MapChipField::Rect& playerRect) {
	float halfWidth = kWidth / 2.0f;
	float halfHeight = kHeight / 2.0f;

	// 重力を加える（落下速度に上限を設ける）
	throwVelocity_.y = (std::max)(throwVelocity_.y - kThrowGravity, -kThrowMaxFallSpeed);

	Vector3 nextPosition = worldTransform_.translation_ + throwVelocity_;

	// 下降中のみ着地判定を行う（上昇中はブロックにぶつかるかどうかだけ見る）
	if (throwVelocity_.y <= 0.0f) {
		float nowBottom = worldTransform_.translation_.y - halfHeight;
		float nextBottom = nextPosition.y - halfHeight;
		bool overlapPlayerX = !(nextPosition.x + halfWidth <= playerRect.left || nextPosition.x - halfWidth >= playerRect.right);

		/// --- 自機の頭の上に着地する ---
		if (overlapPlayerX && nowBottom >= playerRect.top - kLandingBlank && nextBottom <= playerRect.top) {
			worldTransform_.translation_.x = nextPosition.x;
			worldTransform_.translation_.y = playerRect.top + halfHeight;
			throwVelocity_.y = 0.0f;
			// 着地したら横方向の勢いも止める（そのままだと滑り続けてしまうため）
			throwVelocity_.x = 0.0f;
			return;
		}

		/// --- ブロックの上に着地する（左下・右下の2点で判定） ---
		if (mapChipField_ != nullptr) {
			MapChipField::IndexSet indexLeft = mapChipField_->GetMapChipIndexByPosition({nextPosition.x - halfWidth, nextBottom, nextPosition.z});
			MapChipField::IndexSet indexRight = mapChipField_->GetMapChipIndexByPosition({nextPosition.x + halfWidth, nextBottom, nextPosition.z});
			bool hitLeft = mapChipField_->GetMapChipTypeByIndex(indexLeft.xIndex, indexLeft.yIndex) == MapChipType::kBlock;
			bool hitRight = mapChipField_->GetMapChipTypeByIndex(indexRight.xIndex, indexRight.yIndex) == MapChipType::kBlock;
			if (hitLeft || hitRight) {
				MapChipField::IndexSet hitIndex = hitLeft ? indexLeft : indexRight;
				MapChipField::Rect blockRect = mapChipField_->GetRectByIndex(hitIndex.xIndex, hitIndex.yIndex);
				worldTransform_.translation_.x = nextPosition.x;
				worldTransform_.translation_.y = blockRect.top + halfHeight;
				throwVelocity_.y = 0.0f;
				// 着地したら横方向の勢いも止める（そのままだと滑り続けてしまうため）
				throwVelocity_.x = 0.0f;
				return;
			}
		}
	} else if (mapChipField_ != nullptr && IsCollidingWithBlock(nextPosition, mapChipField_)) {
		// 上昇中にブロックへ頭をぶつけたら、その場で速度だけ止める（仮実装）
		throwVelocity_.y = 0.0f;
		nextPosition.y = worldTransform_.translation_.y;
	}

	// 左右方向のブロック衝突は今回は考慮していない（仮実装。必要になったら追加する）
	worldTransform_.translation_ = nextPosition;
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

///// ----- 投げる処理 ----- /////
void CloneBase::Throw(const Vector3& velocity) {
	throwVelocity_ = velocity;
	isThrown_ = true;
}

bool CloneBase::ConsumeWaterDestroyed() {
	if (!wasDestroyedByWater_) {
		return false;
	}

	wasDestroyedByWater_ = false;
	return true;
}