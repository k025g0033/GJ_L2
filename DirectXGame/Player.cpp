#define NOMINMAX

#include "Player.h"
#include "MapChipField.h"
#include "WorldTransformConfig.h"
#include "math/MathUtility.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

void Player::Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position) {
	// modelはnullptrでも構わない（ベースモデルを使わず、SetExtraPartModelsで設定したパーツだけで
	// 見た目を構成する場合。例：自機は頭・左腕・右腕のパーツのみで立方体のベースモデルは使わない）

	// 引数の値をメンバ変数にコピー
	model_ = model;
	camera_ = camera;

	worldTransform_.rotation_.y = std::numbers::pi_v<float> / 2.0f;

	// ワールド変換の初期化z
	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	// モデルの表示スケールを反映（当たり判定サイズには影響しない、見た目だけの拡大縮小）
	worldTransform_.scale_ = {modelScale_, modelScale_, modelScale_};
}

void Player::Update(bool canMove, const std::vector<MapChipField::Rect>& obstacleRects) {

	CheckInWater();
	if (canMove) {
		Move();
	} else {
		// 操作は受け付けないが、重力（落下）だけは働かせる
		ApplyGravityOnly();
	}

	// 衝突情報を初期化
	Player::CollisionMapInfo collisionInfo;
	// 移動量に速度をコピー
	collisionInfo.moveVelocity = velocity_;

	/// --- マップ衝突チェック ---
	isMapCollision(collisionInfo);

	// クローンの素など、ブロック以外の障害物との当たり判定
	// ブロックで補正した後の移動量に対してさらに補正する
	isObstacleCollision(collisionInfo, obstacleRects);

	isCollisionMove(collisionInfo);

	isHitCeiling(collisionInfo);

	isOnGround(collisionInfo, obstacleRects);

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

	// モデルの表示スケールを反映（ImGuiで変更された場合もここで毎フレーム反映される）
	worldTransform_.scale_ = {modelScale_, modelScale_, modelScale_};

	// 行列を定数バッファに転送
	UpdateWorldTransform(worldTransform_);
}

///// ----- モデルの表示スケール ----- /////
// Update()を通さずに、その場でスケールを反映する。
// 変形アニメーション中はUpdate()を呼ばない（＝動かさない）ので、こちらで見た目だけを更新する。
void Player::SetModelScaleImmediate(float scale) {
	modelScale_ = scale;
	worldTransform_.scale_ = {modelScale_, modelScale_, modelScale_};
	UpdateWorldTransform(worldTransform_);
}

void Player::Draw(ObjectColor* objectColor) {
	// ベースモデルを描画（持っている間はholdingModel_があればそちらを使う。未設定ならmodel_のまま）
	// ※ベースモデル自体が未設定（nullptr）の場合は、追加パーツ（頭・腕など）だけで見た目を構成する
	// 　ということなので、ここでは何も描画しない
	Model* modelToDraw = (isHolding_ && holdingModel_ != nullptr) ? holdingModel_ : model_;
	if (modelToDraw != nullptr) {
		modelToDraw->Draw(worldTransform_, *camera_, objectColor);
	}

	// 頭・腕など、ベースに重ねて描画する追加パーツ（設定されていれば）。
	// 原点をワールド原点に合わせてエクスポートしてあるので、ベースと同じworldTransform_で
	// そのまま描画するだけで正しい位置に組み合わさる。
	for (Model* partModel : extraPartModels_) {
		if (partModel != nullptr) {
			partModel->Draw(worldTransform_, *camera_, objectColor);
		}
	}
}

///// ----- 操作を受け付けない間の落下処理 ----- /////
// クローンの変形アニメーション中やリンク線を撃っている最中など、
// 操作を止めている間でも重力だけは働かせる。
// ※以前は velocity_ をまるごと0にしていたため、クローンの素の上に乗った状態で
// 　別の素にリンクすると、足場が無くなっても自機がその場に浮いたままになっていた。
void Player::ApplyGravityOnly() {
	// 横方向の入力は受け付けないので、その場で止める
	velocity_.x = 0.0f;

	if (isInWater_) {
		// 水中はMoveInWaterと同じ扱いで、弱い重力でゆっくり沈む
		velocity_.y -= kWaterGravity;
		velocity_.y = std::clamp(velocity_.y, -kLimitWaterFallSpeed, kSwimSpeedY);
		onGround_ = false;
		return;
	}

	// 接地していない間だけ落下させる（ジャンプ入力は受け付けない）
	if (!onGround_) {
		velocity_.y += -kGravityAcceleration;
		velocity_.y = std::max(velocity_.y, -kLimitFallSpeed);
	}
}

void Player::Move() {
	// 移動入力

	if (isInWater_) {
		MoveInWater();
		return;
	}

	// 水面を上向きに抜けた直後は、短い猶予内のW入力で岸へ飛び出せる。
	// 押しっぱなしで水面を抜けた場合も、そのフレームで成立する。
	const bool preserveWaterExitMomentum = waterExitGraceFrames_ > 0;
	if (waterExitGraceFrames_ > 0) {
		if (canJump_ && Input::GetInstance()->PushKey(DIK_W)) {
			velocity_.y = std::max(velocity_.y, kWaterExitJumpSpeed);
			waterExitGraceFrames_ = 0;
			onGround_ = false;
		} else {
			--waterExitGraceFrames_;
		}
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
	} else if (preserveWaterExitMomentum) {
		// 水から出た直後にキーを離しても、水中で付いた横方向の勢いを急に消さない。
		velocity_.x *= 1.0f - kWaterResistance;
	} else {
		velocity_.x = 0.0f;
	}
	if (onGround_) {
		if (canJump_ && Input::GetInstance()->PushKey(DIK_W)) {
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

// ※斜め移動（X・Y同時の移動）に対応するため、
// 「ブロックがあるかどうか」の判定は移動後の角(斜め込み)で行うが、
// 「実際にどこまで進めるか」を求める限界座標(limit)の計算は、判定している軸の移動量だけを使って
// 現在のX(またはY)座標のまま求める。こうすることでX・Y各方向の判定が互いに影響し合わなくなり、
// 斜めから侵入したときにブロックへめり込む・すり抜けることを防げる。
void Player::isMapCollisionTop(CollisionMapInfo& info) {

	// 上昇あり？
	if (info.moveVelocity.y <= 0.0f) {
		return;
	}

	// 移動後の4つの角の計算（ヒット判定用）
	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(info.moveVelocity);

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
		// Y軸方向の移動量だけを使い、現在のX座標のままこの先どのマスに入るかを求める
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.y += info.moveVelocity.y + (kHeight / 2.0f); // 中心 + 移動量 + 半径 = 未来の上端座標
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.y += (kHeight / 2.0f); // 中心 + 半径 = 現在の上端座標
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		// 現在と未来でマス目（Yインデックス）をまたいだ場合のみ処理する
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

	// 移動後の4つの角の計算（ヒット判定用）
	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(info.moveVelocity);

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
		// Y軸方向の移動量だけを使い、現在のX座標のままこの先どのマスに入るかを求める
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.y += info.moveVelocity.y - (kHeight / 2.0f); // 中心 + 移動量 - 半径 = 未来の下端座標
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.y -= (kHeight / 2.0f); // 中心 - 半径 = 現在の下端座標
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		// 現在と未来でマス目（Yインデックス）をまたいだ場合のみ処理する
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

	// 移動後の4つの角の計算（ヒット判定用）
	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(info.moveVelocity);

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
		// X軸方向の移動量だけを使い、現在のY座標のままこの先どのマスに入るかを求める
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.x += info.moveVelocity.x + GetRightHalfWidth(); // 中心 + 移動量 + 右半幅 = 未来の右端座標
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.x += GetRightHalfWidth(); // 中心 + 右半幅 = 現在の右端座標
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		// 現在と未来でマス目（Xインデックス）をまたいだ場合のみ処理する
		if (indexSetNow.xIndex != indexSet.xIndex) {
			// めり込み先ブロックの範囲矩形
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			info.moveVelocity.x = std::max(rect.left - worldTransform_.translation_.x - GetRightHalfWidth() - kBlank, 0.0f);
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

	// 移動後の4つの角の計算（ヒット判定用）
	std::array<Vector3, kNumCorner> positionsNew = GetCalculatedCorners(info.moveVelocity);

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
		// X軸方向の移動量だけを使い、現在のY座標のままこの先どのマスに入るかを求める
		Vector3 nextPos = worldTransform_.translation_;
		nextPos.x += info.moveVelocity.x - GetLeftHalfWidth(); // 中心 + 移動量 - 左半幅 = 未来の左端座標
		indexSet = mapChipField_->GetMapChipIndexByPosition(nextPos);

		Vector3 nowPos = worldTransform_.translation_;
		nowPos.x -= GetLeftHalfWidth(); // 中心 - 左半幅 = 現在の左端座標
		MapChipField::IndexSet indexSetNow = mapChipField_->GetMapChipIndexByPosition(nowPos);

		// 現在と未来でマス目（Xインデックス）をまたいだ場合のみ処理する
		if (indexSetNow.xIndex != indexSet.xIndex) {
			// めり込み先ブロックの範囲矩形
			MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);
			info.moveVelocity.x = std::min(rect.right - worldTransform_.translation_.x + GetLeftHalfWidth() + kBlank, 0.0f);
			// 壁に当たったことを判定結果に記録する
			info.isWallCollision = true;
		}
	}
}

KamataEngine::Vector3 Player::CornerPosition(const KamataEngine::Vector3& center, Corner corner) {
	// 左右非対称（持っている間は向いている方向側だけが伸びる）に対応するため、
	// 右側の角はGetRightHalfWidth()、左側の角はGetLeftHalfWidth()を使う
	Vector3 offsetTable[kNumCorner] = {
	    {GetRightHalfWidth(),  -kHeight / 2.0f, 0.0f}, // 右下
	    {-GetLeftHalfWidth(),  -kHeight / 2.0f, 0.0f}, // 左下
	    {GetRightHalfWidth(),  kHeight / 2.0f,  0.0f}, // 右上
	    {-GetLeftHalfWidth(),  kHeight / 2.0f,  0.0f}, // 左上
	};

	return center + offsetTable[static_cast<int>(corner)];
}

// 中心から見た左右それぞれの半幅
// 通常時(isHolding_==false)はGetWidth()==kWidthなので、frontHalf==backHalf==kWidth/2となり
// 今まで通りの左右対称になる。持っている間は「背中側」をkWidth/2に固定したまま、
// 「向いている方向側」だけをGetWidth()まで伸ばす。
float Player::GetLeftHalfWidth() const {
	float backHalf = kWidth / 2.0f;
	float frontHalf = GetWidth() - backHalf;
	return (lrDirection_ == LRDirection::kRight) ? backHalf : frontHalf;
}

float Player::GetRightHalfWidth() const {
	float backHalf = kWidth / 2.0f;
	float frontHalf = GetWidth() - backHalf;
	return (lrDirection_ == LRDirection::kRight) ? frontHalf : backHalf;
}

std::array<KamataEngine::Vector3, Player::kNumCorner> Player::GetCalculatedCorners(const KamataEngine::Vector3& moveAmount) {
	std::array<Vector3, kNumCorner> positionsNew;

	// 現在の座標 ＋ 指定された移動量 ＝ 未来の中心座標
	Vector3 nextCenter = worldTransform_.translation_ + moveAmount;

	for (uint32_t i = 0; i < kNumCorner; ++i) {
		positionsNew[i] = CornerPosition(nextCenter, static_cast<Corner>(i));
	}

	return positionsNew;
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

void Player::isOnGround(const CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects) {
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

		// マップブロック上でなければ、クローンの素・扉・動く足場などの
		// 動的な障害物の上に乗っているかも調べる。
		if (!hit) {
			float left = worldTransform_.translation_.x - GetLeftHalfWidth();
			float right = worldTransform_.translation_.x + GetRightHalfWidth();
			float playerBottom = worldTransform_.translation_.y - kHeight / 2.0f;

			for (const MapChipField::Rect& rect : obstacleRects) {
				bool overlapX = !(right <= rect.left || left >= rect.right);
				bool touchingTop = std::abs(playerBottom - rect.top) <= kGroundSearchHeight;
				if (overlapX && touchingTop) {
					hit = true;
					break;
				}
			}
		}

		// 全種類の足元判定が終わってから接地状態を確定する。
		// 先にfalseへすると、動く足場上で接地と空中を毎フレーム繰り返してしまう。
		onGround_ = hit;

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
	wasInWater_ = isInWater_;
	MapChipField::IndexSet index = mapChipField_->GetMapChipIndexByPosition(worldTransform_.translation_);

	isInWater_ = mapChipField_->GetMapChipTypeByIndex(index.xIndex, index.yIndex) == MapChipType::kWater;

	// 上昇中に水判定が外れた時だけ、離水ジャンプの入力猶予を開始する。
	if (wasInWater_ && !isInWater_ && velocity_.y > 0.0f) {
		waterExitGraceFrames_ = kWaterExitGraceFrameCount;
		onGround_ = false;
	}
	if (isInWater_) {
		waterExitGraceFrames_ = 0;
	}
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
///// ----- クローンの素など、マップチップ以外の障害物との当たり判定 ----- /////
void Player::isObstacleCollision(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects) {
	// 先に横方向を確定させ、そのあと「横に動いた後の位置」で縦方向を判定する。
	// ※以前は縦→横の順で、しかもどちらも「動く前の位置」で重なりを見ていたため、
	// 　斜めに進むと縦も横も「まだ重なっていない」と判定されて、障害物の角から中へ入り込めてしまっていた。
	isObstacleCollisionRight(info, obstacleRects);
	isObstacleCollisionLeft(info, obstacleRects);
	isObstacleCollisionTop(info, obstacleRects);
	isObstacleCollisionBottom(info, obstacleRects);
}

void Player::isObstacleCollisionTop(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects) {
	if (info.moveVelocity.y <= 0.0f) {
		return;
	}

	float halfHeight = kHeight / 2.0f;
	// 横方向はすでに確定しているので、「横に動いた後」のX座標で重なりを判定する
	float movedX = worldTransform_.translation_.x + info.moveVelocity.x;
	float nowLeft = movedX - GetLeftHalfWidth();
	float nowRight = movedX + GetRightHalfWidth();
	float nowTop = worldTransform_.translation_.y + halfHeight;

	for (const MapChipField::Rect& rect : obstacleRects) {
		if (nowRight <= rect.left || nowLeft >= rect.right) {
			continue; // 横方向が重なっていない障害物は無視する
		}

		float newTop = nowTop + info.moveVelocity.y;
		if (nowTop <= rect.bottom && newTop > rect.bottom) {
			// すでに天井に接している場合、そのまま引くとマイナス（＝下向き）になってしまう。
			// 0で止めることで「上に進めないだけ」にして、下へ押し込まれないようにする。
			info.moveVelocity.y = (std::max)(0.0f, rect.bottom - nowTop - kBlank);
			info.isCeilingCollision = true;
		}
	}
}

void Player::isObstacleCollisionBottom(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects) {
	if (info.moveVelocity.y >= 0.0f) {
		return;
	}

	float halfHeight = kHeight / 2.0f;
	// 横方向はすでに確定しているので、「横に動いた後」のX座標で重なりを判定する
	float movedX = worldTransform_.translation_.x + info.moveVelocity.x;
	float nowLeft = movedX - GetLeftHalfWidth();
	float nowRight = movedX + GetRightHalfWidth();
	float nowBottom = worldTransform_.translation_.y - halfHeight;

	for (const MapChipField::Rect& rect : obstacleRects) {
		if (nowRight <= rect.left || nowLeft >= rect.right) {
			continue;
		}

		float newBottom = nowBottom + info.moveVelocity.y;
		if (nowBottom >= rect.top && newBottom < rect.top) {
			// 下向きの補正が上向き（プラス）に転じないように0で止める
			info.moveVelocity.y = (std::min)(0.0f, rect.top - nowBottom + kBlank);
			info.isGroundCollision = true;
		}
	}
}

void Player::isObstacleCollisionRight(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects) {
	if (info.moveVelocity.x <= 0.0f) {
		return;
	}

	float halfHeight = kHeight / 2.0f;
	float nowBottom = worldTransform_.translation_.y - halfHeight;
	float nowTop = worldTransform_.translation_.y + halfHeight;
	float nowRight = worldTransform_.translation_.x + GetRightHalfWidth();

	for (const MapChipField::Rect& rect : obstacleRects) {
		if (nowTop <= rect.bottom || nowBottom >= rect.top) {
			continue; // 縦方向が重なっていない障害物は無視する
		}

		float newRight = nowRight + info.moveVelocity.x;
		if (nowRight <= rect.left && newRight > rect.left) {
			// 右向きの補正が左向き（マイナス）に転じないように0で止める
			info.moveVelocity.x = (std::max)(0.0f, rect.left - nowRight - kBlank);
			info.isWallCollision = true;
		}
	}
}

void Player::isObstacleCollisionLeft(CollisionMapInfo& info, const std::vector<MapChipField::Rect>& obstacleRects) {
	if (info.moveVelocity.x >= 0.0f) {
		return;
	}

	float halfHeight = kHeight / 2.0f;
	float nowBottom = worldTransform_.translation_.y - halfHeight;
	float nowTop = worldTransform_.translation_.y + halfHeight;
	float nowLeft = worldTransform_.translation_.x - GetLeftHalfWidth();

	for (const MapChipField::Rect& rect : obstacleRects) {
		if (nowTop <= rect.bottom || nowBottom >= rect.top) {
			continue;
		}

		float newLeft = nowLeft + info.moveVelocity.x;
		if (nowLeft >= rect.right && newLeft < rect.right) {
			// 左向きの補正が右向き（プラス）に転じないように0で止める
			info.moveVelocity.x = (std::min)(0.0f, rect.right - nowLeft + kBlank);
			info.isWallCollision = true;
		}
	}
}

void Player::Respawn(const Vector3& position) {
	// 初期位置へ戻す
	worldTransform_.translation_ = position;

	// 移動速度をリセット
	velocity_ = {};

	// 状態をリセット
	onGround_ = false;
	isInWater_ = false;
	wasInWater_ = false;
	waterExitGraceFrames_ = 0;
	turnTimer_ = 0.0f;

	UpdateWorldTransform(worldTransform_);
}
