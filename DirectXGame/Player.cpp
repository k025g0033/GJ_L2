#define NOMINMAX

#include "Player.h"
#include "MapChipField.h"
#include "WorldTransformConfig.h"
#include "math/MathUtility.h"
#include <algorithm>
#include <cassert>
#include <numbers>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

void Player::Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position) {
	// NILLポインタチェック
	assert(model);

	// 引数の値をメンバ変数にコピー
	model_ = model;
	camera_ = camera;

	worldTransform_.rotation_.y = std::numbers::pi_v<float> / 2.0f;

	// ワールド変換の初期化z
	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
}

void Player::Update(bool canMove) {

	CheckInWater();
	if (canMove) {
		Move();
	} else {
		velocity_ = {};
	}

	// 衝突情報を初期化
	Player::CollisionMapInfo collisionInfo;
	// 移動量に速度をコピー
	collisionInfo.moveVelocity = velocity_;

	// マップ衝突チェック
	isMapCollision(collisionInfo);

	isCollisionMove(collisionInfo);

	isHitCeiling(collisionInfo);

	isOnGround(collisionInfo);

	isHitWall(collisionInfo);

	// 旋回制御
	if (turnTimer_ > 0.0f) {
		turnTimer_ -= 1.0f / 60.0f;

		// 左右の自キャラ角度テーブル
		float destinationRotationYTable[] = {
		    std::numbers::pi_v<float> / 2.0f,       // 右向き
		    std::numbers::pi_v<float> * 3.0f / 2.0f // 左向き
		};
		// 状態に応じた角度を取得する
		float destinationRotationY = destinationRotationYTable[static_cast<uint32_t>(lrDirection_)];

		// 補間割合（0.0〜1.0）を計算。1.0からタイマーを引くことで「0から1へ進む」値になります
		float t = (0.3f - turnTimer_) / 0.3f;

		// 自キャラの角度を設定する
		worldTransform_.rotation_.y = turnFirstRotationY_ + (destinationRotationY - turnFirstRotationY_) * t;
	}

	// 行列を定数バッファに転送
	UpdateWorldTransform(worldTransform_);
}

void Player::Draw() {
	// 3Dモデルを描画
	model_->Draw(worldTransform_, *camera_);
}

void Player::Move() {
	// 移動入力

	if (isInWater_) {
		MoveInWater();
		return;
	}

	// 地上移動操作
	if (Input::GetInstance()->PushKey(DIK_D) || Input::GetInstance()->PushKey(DIK_A)) {

		if (Input::GetInstance()->PushKey(DIK_D)) {
			velocity_.x = kLimitRunSpeed;

			if (lrDirection_ != LRDirection::kRight) {
				lrDirection_ = LRDirection::kRight;
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				turnTimer_ = kTimeTurn;
			}
		} else if (Input::GetInstance()->PushKey(DIK_A)) {
			velocity_.x = -kLimitRunSpeed;

			if (lrDirection_ != LRDirection::kLeft) {
				lrDirection_ = LRDirection::kLeft;
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				turnTimer_ = kTimeTurn;
			}
		}
	} else {
		velocity_.x = 0.0f;
	}
	if (onGround_) {
		if (Input::GetInstance()->PushKey(DIK_W)) {
			// ジャンプ初速
			velocity_.y += kJumpAcceleration;
		}
	} else {
		// 落下速度
		velocity_.y += -kGravityAcceleration;
		// 落下速度制限
		velocity_.y = std::max(velocity_.y, -kLimitFallSpeed);
	}
}

void Player::isMapCollision(CollisionMapInfo& info) {
	isMapCollisionTop(info);
	isMapCollisionBottom(info);
	isMapCollisionRight(info);
	isMapCollisionLeft(info);
}

void Player::isMapCollisionTop(CollisionMapInfo& info) {

	// 上昇あり？
	if (info.moveVelocity.y <= 0.0f) {
		return;
	}

	// 移動前の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNow;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNow[i] = CornerPosition(worldTransform_.translation_, static_cast<Corner>(i));
	}

	// 移動後の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNew;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + info.moveVelocity, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	// 真上の当たり判定を行う
	bool hit = false;
	// 左上点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex + 1);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右上点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex + 1);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// ブロックにヒット？
	if (hit) {

		// まずは左上をデフォルトとする
		KamataEngine::Vector3 hitPosition = positionsNew[kLeftTop];
		KamataEngine::Vector3 nowPosition = positionsNow[kLeftTop];

		// もし右上だけが当たっていた、あるいは右上がブロックなら右上の座標を使う
		MapChipField::IndexSet indexRightTop = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
		if (mapChipField_->GetMapChipTypeByIndex(indexRightTop.xIndex, indexRightTop.yIndex) == MapChipType::kBlock) {
			hitPosition = positionsNew[kRightTop];
			nowPosition = positionsNow[kRightTop];
		}

		// めり込みを排除する方向に移動量を設定する
		indexSet = mapChipField_->GetMapChipIndexByPosition(hitPosition);
		// 現在座標が壁の外か判定
		MapChipField::IndexSet indexSetNow;
		indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPosition);
		if (indexSetNow.yIndex != indexSet.yIndex) {
			// めり込み先ブロックの範囲矩形
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			info.moveVelocity.y = std::max(0.0f, rect.bottom - worldTransform_.translation_.y - kHeight / 2.0f - kBlank);
			// 天井に当たったことを記録する
			info.isCeilingCollision = true;
		}
	}
}

void Player::isMapCollisionBottom(CollisionMapInfo& info) {
	// 下降あり？
	if (info.moveVelocity.y >= 0.0f) {
		return;
	}

	// 移動前の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNow;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNow[i] = CornerPosition(worldTransform_.translation_, static_cast<Corner>(i));
	}

	// 移動後の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNew;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + info.moveVelocity, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	// 真下の当たり判定を行う
	bool hit = false;
	// 左下点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex - 1);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右下点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex - 1);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// ブロックにヒット？
	if (hit) {
		// まずは左下をデフォルトとする
		KamataEngine::Vector3 hitPosition = positionsNew[kLeftBottom];
		KamataEngine::Vector3 nowPosition = positionsNow[kLeftBottom];

		// もし右下だけが当たっていた、あるいは右下がブロックなら右下の座標を使う
		MapChipField::IndexSet indexRightBottom = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
		if (mapChipField_->GetMapChipTypeByIndex(indexRightBottom.xIndex, indexRightBottom.yIndex) == MapChipType::kBlock) {
			hitPosition = positionsNew[kRightBottom];
			nowPosition = positionsNow[kRightBottom];
		}

		// めり込みを排除する方向に移動量を設定する
		indexSet = mapChipField_->GetMapChipIndexByPosition(hitPosition);
		// 現在座標が壁の外か判定
		MapChipField::IndexSet indexSetNow;
		indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPosition);
		if (indexSetNow.yIndex != indexSet.yIndex) {
			// めり込み先ブロックの範囲矩形
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			info.moveVelocity.y = std::min(0.0f, rect.top - worldTransform_.translation_.y + kHeight / 2.0f + kBlank);
			// 地面に当たったことを記録する
			info.isGroundCollision = true;
		}
	}
}

void Player::isMapCollisionRight(CollisionMapInfo& info) {
	// 右移動あり？
	if (info.moveVelocity.x <= 0.0f) {
		return;
	}

	// 移動前の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNow;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNow[i] = CornerPosition(worldTransform_.translation_, static_cast<Corner>(i));
	}

	// 移動後の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNew;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + info.moveVelocity, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	// 壁の当たり判定を行う
	bool hit = false;
	// 右上点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex - 1, indexSet.yIndex);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 右下点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex - 1, indexSet.yIndex);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// ブロックにヒット？
	if (hit) {
		// まずは右上をデフォルトとする
		KamataEngine::Vector3 hitPosition = positionsNew[kRightTop];
		KamataEngine::Vector3 nowPosition = positionsNow[kRightTop];

		// もし右下だけが当たっていた、あるいは右下がブロックなら右下の座標を使う
		MapChipField::IndexSet indexRightBottom = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom]);
		if (mapChipField_->GetMapChipTypeByIndex(indexRightBottom.xIndex, indexRightBottom.yIndex) == MapChipType::kBlock) {
			hitPosition = positionsNew[kRightBottom];
			nowPosition = positionsNow[kRightBottom];
		}

		// めり込みを排除する方向に移動量を設定する
		indexSet = mapChipField_->GetMapChipIndexByPosition(hitPosition);
		// 現在座標が壁の外か判定
		MapChipField::IndexSet indexSetNow;
		indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPosition);
		if (indexSetNow.xIndex != indexSet.xIndex) {
			// めり込み先ブロックの範囲矩形
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			info.moveVelocity.x = std::max(rect.left - worldTransform_.translation_.x - kWidth / 2.0f - kBlank, 0.0f);
			// 壁に当たったことを判定結果に記録する
			info.isWallCollision = true;
		}
	}
}

void Player::isMapCollisionLeft(CollisionMapInfo& info) {
	// 左移動あり？
	if (info.moveVelocity.x >= 0.0f) {
		return;
	}

	// 移動前の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNow;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNow[i] = CornerPosition(worldTransform_.translation_, static_cast<Corner>(i));
	}

	// 移動後の4つの角の計算
	std::array<Vector3, kNumCorner> positionsNew;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + info.moveVelocity, static_cast<Corner>(i));
	}

	MapChipType mapChipType;
	MapChipType mapChipTypeNext;
	// 壁の当たり判定を行う
	bool hit = false;
	// 左上点の判定
	MapChipField::IndexSet indexSet;
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftTop]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex + 1, indexSet.yIndex);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// 左下点の判定
	indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
	mapChipType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
	mapChipTypeNext = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex + 1, indexSet.yIndex);
	// 隣接セルがともにブロックであればヒット
	if (mapChipType == MapChipType::kBlock && mapChipTypeNext != MapChipType::kBlock) {
		hit = true;
	}

	// ブロックにヒット？
	if (hit) {
		// まずは左上をデフォルトとする
		KamataEngine::Vector3 hitPosition = positionsNew[kLeftTop];
		KamataEngine::Vector3 nowPosition = positionsNow[kLeftTop];

		// もし右下だけが当たっていた、あるいは右下がブロックなら右下の座標を使う
		MapChipField::IndexSet indexRightBottom = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom]);
		if (mapChipField_->GetMapChipTypeByIndex(indexRightBottom.xIndex, indexRightBottom.yIndex) == MapChipType::kBlock) {
			hitPosition = positionsNew[kLeftBottom];
			nowPosition = positionsNow[kLeftBottom];
		}

		// めり込みを排除する方向に移動量を設定する
		indexSet = mapChipField_->GetMapChipIndexByPosition(hitPosition);
		// 現在座標が壁の外か判定
		MapChipField::IndexSet indexSetNow;
		indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPosition);
		if (indexSetNow.xIndex != indexSet.xIndex) {
			// めり込み先ブロックの範囲矩形
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			info.moveVelocity.x = std::min(rect.right - worldTransform_.translation_.x + kWidth / 2.0f + kBlank, 0.0f);
			// 壁に当たったことを判定結果に記録する
			info.isWallCollision = true;
		}
	}
}

KamataEngine::Vector3 Player::CornerPosition(const KamataEngine::Vector3& center, Corner corner) {
	Vector3 offsetTable[kNumCorner] = {
	    {kWidth / 2.0f,  -kHeight / 2.0f, 0.0f}, // 右下
	    {-kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // 左下
	    {kWidth / 2.0f,  kHeight / 2.0f,  0.0f}, // 右上
	    {-kWidth / 2.0f, kHeight / 2.0f,  0.0f}, // 左上
	};

	return center + offsetTable[static_cast<int>(corner)];
}

void Player::isCollisionMove(const CollisionMapInfo& info) {
	// 移動
	worldTransform_.translation_.x += info.moveVelocity.x;
	worldTransform_.translation_.y += info.moveVelocity.y;
	worldTransform_.translation_.z += info.moveVelocity.z;
}

void Player::isHitCeiling(const CollisionMapInfo& info) {
	// 天井に当たった？
	if (info.isCeilingCollision) {
		DebugText::GetInstance()->ConsolePrintf("hit ceiling\n");
		velocity_.y = 0;
	}
}

void Player::isOnGround(const CollisionMapInfo& info) {
	if (onGround_) {
		// ジャンプ開始
		if (velocity_.y > 0.0f) {
			onGround_ = false;
			return;
		}

		std::array<Vector3, kNumCorner> positionsNew;

		for (uint32_t i = 0; i < kNumCorner; ++i) {
			positionsNew[i] = CornerPosition(worldTransform_.translation_, static_cast<Corner>(i));
		}

		bool hit = false;
		MapChipField::IndexSet indexSet;

		indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kLeftBottom] + Vector3(0.0f, -kGroundSearchHeight, 0.0f));

		if (mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex) == MapChipType::kBlock) {
			hit = true;
		}

		indexSet = mapChipField_->GetMapChipIndexByPosition(positionsNew[kRightBottom] + Vector3(0.0f, -kGroundSearchHeight, 0.0f));

		if (mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex) == MapChipType::kBlock) {
			hit = true;
		}

		if (!hit) {
			onGround_ = false;
		}

	} else {
		// 落下中に地面へ衝突
		if (info.isGroundCollision) {
			onGround_ = true;
			velocity_.y = 0.0f;
		}
	}
}

void Player::isHitWall(const CollisionMapInfo& info) {
	// 壁接触による減速
	if (info.isWallCollision) {
		velocity_.x *= (1.0f - kAttenuationWall);
	}
}

///// ----- 方向転換キャンセル ----- /////
void Player::CancelTurn() {
	// 向きを元(逆)に戻す
	lrDirection_ = (lrDirection_ == LRDirection::kRight) ? LRDirection::kLeft : LRDirection::kRight;

	// 現在の(旋回途中の)角度から、戻す方向への旋回アニメーションを開始する
	turnFirstRotationY_ = worldTransform_.rotation_.y;
	turnTimer_ = kTimeTurn;
}

void Player::CheckInWater() {
	MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(worldTransform_.translation_);

	isInWater_ = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex) == MapChipType::kWater;
}

void Player::MoveInWater() {
	if (Input::GetInstance()->PushKey(DIK_D) || Input::GetInstance()->PushKey(DIK_A)) {

		if (Input::GetInstance()->PushKey(DIK_D)) {
			velocity_.x = kSwimSpeedX;

			if (lrDirection_ != LRDirection::kRight) {
				lrDirection_ = LRDirection::kRight;
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				turnTimer_ = kTimeTurn;
			}
		} else if (Input::GetInstance()->PushKey(DIK_A)) {
			velocity_.x = -kSwimSpeedX;

			if (lrDirection_ != LRDirection::kLeft) {
				lrDirection_ = LRDirection::kLeft;
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				turnTimer_ = kTimeTurn;
			}
		}
	} else {
		velocity_.x *= 1.0f - kWaterResistance;
		if (std::abs(velocity_.x) < 0.001f) {
			velocity_.x = 0.0f;
		}
	}

	// 弱い重力で、上昇後はゆっくり沈む。
	velocity_.y -= kWaterGravity;
	// 押した瞬間だけひとかきする（押しっぱなしでは連続上昇しない）。
	if (Input::GetInstance()->TriggerKey(DIK_W)) {
		velocity_.y = kSwimSpeedY;
	}

	velocity_.x = std::clamp(velocity_.x, -kSwimSpeedX, kSwimSpeedX);

	velocity_.y = std::clamp(velocity_.y, -kLimitWaterFallSpeed, kSwimSpeedY);

	// 1水中では地上扱いにしない
	onGround_ = false;
}
