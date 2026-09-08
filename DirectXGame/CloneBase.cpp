#include "CloneBase.h"
#include "MapChipField.h"
#include "WorldTransformConfig.h"

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

CloneBase::~CloneBase() { delete player_; }

void CloneBase::Initialize(Model* modelBase, Model* modelClone, Camera* camera, MapChipField* mapChipField, const Vector3& position) {
	modelBase_ = modelBase;
	modelClone_ = modelClone;
	camera_ = camera;
	mapChipField_ = mapChipField;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};
	initialPosition_ = position; // 初期位置を保存

	chargeColor_.Initialize();
	chargeColor_.SetColor({0.2f, 0.7f, 1.0f, 1.0f});

	state_ = State::kBase;

	player_ = new Player();
	player_->Initialize(modelClone_, camera_, position);
	player_->SetMapChipField(mapChipField);
	// クローンはジャンプできないようにする（ジャンプできるのは自機(GameSceneのplayer_)のみ）
	player_->SetCanJump(false);
}

void CloneBase::Update(bool isControlled, const std::vector<MapChipField::Rect>& obstacleRects, const MapChipField::Rect& playerRect) {

	// 帯電時間を減らす
	if (isCharged_) {
		--chargeTimer_;

		if (chargeTimer_ <= 0) {
			Discharge();
		}
	}

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

	Vector3 moveAmount = throwVelocity_;

	// ブロックとの当たり判定（上下左右）を行い、めり込まないように移動量を補正する。
	// X・Y軸をそれぞれ独立して判定するので、斜めに飛んでいてもブロックへめり込んだりすり抜けたりしない。
	BlockCollisionResult blockResult = CheckBlockCollision(moveAmount);

	// 下降中のみ、自機の頭の上に着地できるかどうかも調べる（ブロックと同じ考え方でその場に止める）
	if (moveAmount.y <= 0.0f) {
		float nowBottom = worldTransform_.translation_.y - halfHeight;
		float nextBottom = nowBottom + moveAmount.y;
		float nextX = worldTransform_.translation_.x + moveAmount.x;
		bool overlapPlayerX = !(nextX + halfWidth <= playerRect.left || nextX - halfWidth >= playerRect.right);

		/// --- 自機の頭の上に着地する ---
		if (overlapPlayerX && nowBottom >= playerRect.top - kLandingBlank && nextBottom <= playerRect.top) {
			moveAmount.y = playerRect.top - nowBottom;
			worldTransform_.translation_ = worldTransform_.translation_ + moveAmount;
			throwVelocity_.y = 0.0f;
			// 着地したら横方向の勢いも止める（そのままだと滑り続けてしまうため）
			throwVelocity_.x = 0.0f;
			return;
		}
	}

	// ブロックに着地・接触したら、対応する方向の勢いを止める
	if (blockResult.isGroundHit) {
		throwVelocity_.y = 0.0f;
		// 着地したら横方向の勢いも止める（そのままだと滑り続けてしまうため）
		throwVelocity_.x = 0.0f;
	}
	if (blockResult.isCeilingHit) {
		throwVelocity_.y = 0.0f;
	}
	if (blockResult.isWallHit) {
		throwVelocity_.x = 0.0f;
	}

	worldTransform_.translation_ = worldTransform_.translation_ + moveAmount;
}

///// ----- 持たれている間の移動（ブロックとの当たり判定つき） ----- /////
void CloneBase::MoveHeldTo(const Vector3& targetPosition) {
	// 現在位置から目標位置までの移動量を求め、ブロックにめり込まないよう補正してから移動する
	Vector3 moveAmount = targetPosition - worldTransform_.translation_;
	CheckBlockCollision(moveAmount);
	worldTransform_.translation_ = worldTransform_.translation_ + moveAmount;
}

///// ----- ブロックとの当たり判定（Player::isMapCollision系と同じ考え方） ----- /////
CloneBase::BlockCollisionResult CloneBase::CheckBlockCollision(Vector3& moveAmount) const {
	BlockCollisionResult result;
	if (mapChipField_ == nullptr) {
		return result;
	}

	CheckBlockCollisionTop(moveAmount, result);
	CheckBlockCollisionBottom(moveAmount, result);
	CheckBlockCollisionRight(moveAmount, result);
	CheckBlockCollisionLeft(moveAmount, result);

	return result;
}

void CloneBase::CheckBlockCollisionTop(Vector3& moveAmount, BlockCollisionResult& result) const {
	// 上昇あり？
	if (moveAmount.y <= 0.0f) {
		return;
	}

	// 移動後の4つの角の計算（ヒット判定用）
	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(moveAmount);

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex + 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex + 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		// Y軸方向の移動量だけを使い、現在のX座標のままこの先どのマスに入るかを求める
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.y += moveAmount.y + (kHeight / 2.0f);
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.y += (kHeight / 2.0f);
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		if (indexSetNow.yIndex != indexSet.yIndex) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			moveAmount.y = (std::max)(0.0f, rect.bottom - worldTransform_.translation_.y - kHeight / 2.0f - kBlank);
			result.isCeilingHit = true;
		}
	}
}

void CloneBase::CheckBlockCollisionBottom(Vector3& moveAmount, BlockCollisionResult& result) const {
	// 下降あり？
	if (moveAmount.y >= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(moveAmount);

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex - 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex - 1);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.y += moveAmount.y - (kHeight / 2.0f);
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.y -= (kHeight / 2.0f);
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		if (indexSetNow.yIndex != indexSet.yIndex) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			moveAmount.y = (std::min)(0.0f, rect.top - worldTransform_.translation_.y + kHeight / 2.0f + kBlank);
			result.isGroundHit = true;
		}
	}
}

void CloneBase::CheckBlockCollisionRight(Vector3& moveAmount, BlockCollisionResult& result) const {
	// 右移動あり？
	if (moveAmount.x <= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(moveAmount);

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex - 1, indexSet.yIndex);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex - 1, indexSet.yIndex);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.x += moveAmount.x + (kWidth / 2.0f);
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.x += (kWidth / 2.0f);
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		if (indexSetNow.xIndex != indexSet.xIndex) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			moveAmount.x = (std::max)(rect.left - worldTransform_.translation_.x - kWidth / 2.0f - kBlank, 0.0f);
			result.isWallHit = true;
		}
	}
}

void CloneBase::CheckBlockCollisionLeft(Vector3& moveAmount, BlockCollisionResult& result) const {
	// 左移動あり？
	if (moveAmount.x >= 0.0f) {
		return;
	}

	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(moveAmount);

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	bool hit = false;

	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex + 1, indexSet.yIndex);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex + 1, indexSet.yIndex);
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	if (hit) {
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.x += moveAmount.x - (kWidth / 2.0f);
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.x -= (kWidth / 2.0f);
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		if (indexSetNow.xIndex != indexSet.xIndex) {
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			moveAmount.x = (std::min)(rect.right - worldTransform_.translation_.x + kWidth / 2.0f + kBlank, 0.0f);
			result.isWallHit = true;
		}
	}
}

KamataEngine::Vector3 CloneBase::CornerPosition(const Vector3& center, Corner corner) const {
	Vector3 offsetTable[kNumCorner] = {
	    {kWidth / 2.0f,  -kHeight / 2.0f, 0.0f}, // 右下
	    {-kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // 左下
	    {kWidth / 2.0f,  kHeight / 2.0f,  0.0f}, // 右上
	    {-kWidth / 2.0f, kHeight / 2.0f,  0.0f}, // 左上
	};

	return center + offsetTable[static_cast<int>(corner)];
}

std::array<KamataEngine::Vector3, CloneBase::kNumCorner> CloneBase::GetCalculatedCorners(const Vector3& moveAmount) const {
	std::array<Vector3, kNumCorner> positionsNew;

	Vector3 nextCenter = worldTransform_.translation_ + moveAmount;
	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNew[i] = CornerPosition(nextCenter, static_cast<Corner>(i));
	}

	return positionsNew;
}

void CloneBase::Draw() {
	// 帯電中は色を変える クローンの素、クローン共通
	ObjectColor* color = isCharged_ ? &chargeColor_ : nullptr;

	if (state_ == State::kTransformed) {
		player_->Draw(color);
	} else {
		modelBase_->Draw(worldTransform_, *camera_,color);
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

///// ----- 当たり判定用の矩形を取得する ----- /////
MapChipField::Rect CloneBase::GetRect() const {
	MapChipField::Rect rect;

	if (state_ == State::kTransformed) {
		// 変形中は、中の自機(Player)の現在位置とサイズをそのまま使う（見た目と一致させるため）
		const Vector3& pos = player_->GetWorldTransform().translation_;
		float halfWidth = player_->GetWidth() / 2.0f;
		float halfHeight = player_->GetHeight() / 2.0f;
		rect.left = pos.x - halfWidth;
		rect.right = pos.x + halfWidth;
		rect.bottom = pos.y - halfHeight;
		rect.top = pos.y + halfHeight;
	} else {
		// 素の状態は自分自身の座標とサイズを使う
		const Vector3& pos = worldTransform_.translation_;
		rect.left = pos.x - kWidth / 2.0f;
		rect.right = pos.x + kWidth / 2.0f;
		rect.bottom = pos.y - kHeight / 2.0f;
		rect.top = pos.y + kHeight / 2.0f;
	}

	return rect;
}

bool CloneBase::ConsumeWaterDestroyed() {
	if (!wasDestroyedByWater_) {
		return false;
	}

	wasDestroyedByWater_ = false;
	return true;
}