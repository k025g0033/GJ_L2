#include "CloneBase.h"
#include "MapChipField.h"
#include "WorldTransformConfig.h"

#include <cmath>
#include <numbers>
#include <cmath>

using namespace KamataEngine;
using namespace KamataEngine::MathUtility;

namespace {

///// ----- イージング ----- /////
// 変形アニメーションの速度に遊びを持たせるための補間関数。
// どれも引数tは0.0〜1.0の進行度で、戻り値も0.0〜1.0（EaseOutBackだけ途中で1.0を超える）。

/// --- 始まりと終わりをゆっくり、中間を速くする ---
// 球体の伸び縮みに使う。動き出しと止まり際がなめらかになる。
float EaseInOutSine(float t) { return -(std::cos(std::numbers::pi_v<float> * t) - 1.0f) / 2.0f; }

/// --- 終わりに向かって減速する ---
// 縮んで消える動きに使う。最初に一気に縮み、消える直前がゆっくりになる。
float EaseOutCubic(float t) {
	float inverted = 1.0f - t;
	return 1.0f - inverted * inverted * inverted;
}

/// --- 最初に大きく動き、終わりに向かって減速する ---
// 球体が縮んでいく動きに使う。つながった直後からはっきり小さくなり始める。
float EaseOutSine(float t) { return std::sin(std::numbers::pi_v<float> * t / 2.0f); }

/// --- 少し行き過ぎてから戻る ---
// 0から現れる動きに使う。一瞬大きくなってから収まるので、ぷるんとした出方になる。
float EaseOutBack(float t) {
	const float overshoot = 1.70158f;
	float inverted = t - 1.0f;
	return 1.0f + (overshoot + 1.0f) * inverted * inverted * inverted + overshoot * inverted * inverted;
}

} // namespace

CloneBase::~CloneBase() {
	delete player_;
}

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

	UpdateBehavior();

	// 変形アニメーション中は、その場で見た目だけを変化させる（移動も物理も止める）
	if (IsAnimating()) {
		UpdateTransformAnimation();
		return;
	}

	if (state_ == State::kTransformed) {
		player_->Update(isControlled, obstacleRects);

		if (player_->IsInWater()) {
			// 水に落ちた場合はアニメーションを挟まず、その場で素に戻して初期位置へ戻す
			player_->Respawn(initialPosition_);
			player_->SetModelScaleImmediate(cloneModelScale_);
			cloneDrawScale_ = cloneModelScale_;
			worldTransform_.translation_ = initialPosition_;
			worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};
			state_ = State::kBase;
			wasDestroyedByWater_ = true;
			// 帯電もここで失う
			Discharge();
			UpdateWorldTransform(worldTransform_);
		}
		return;
	}

	// 投げられている（＝重力が働いている）間は、毎フレーム着地判定をやり直す。
	// こうしておくと、自機の上に乗った後で自機が動いた時に、支えがなくなって自然に落下を再開する。
	if (isThrown_) {
		UpdateThrowPhysics(playerRect, obstacleRects);
	}

	// 素の状態の表示スケール
	worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};

	UpdateWorldTransform(worldTransform_);
}

///// ----- 変形（素 <-> クローン） ----- /////
void CloneBase::Transform() {
	// すでにクローンになっている、またはアニメーション中なら何もしない
	if (state_ != State::kBase) {
		return;
	}

	// 変形前（素の状態）の現在位置をそのまま引き継ぐ
	player_->SetTranslation(worldTransform_.translation_);

	// 投げられている最中に変形した場合は、その物理を止める
	throwVelocity_ = {};
	isThrown_ = false;

	state_ = State::kTransforming;
	animationTimer_ = 0;
	animationDuration_ = static_cast<int>(kTransformSeconds * kFramesPerSecond);

	// クローンはまだ見せない（球体が消えてから0から大きくする）
	cloneDrawScale_ = 0.0f;
	player_->SetModelScaleImmediate(0.0f);
}

void CloneBase::ResetToBase() {
	// クローンになっていない、またはアニメーション中なら何もしない
	if (state_ != State::kTransformed) {
		return;
	}

	// 変形中の現在位置をそのまま引き継ぐ
	worldTransform_.translation_ = player_->GetWorldTransform().translation_;

	state_ = State::kReverting;
	animationTimer_ = 0;
	animationDuration_ = static_cast<int>(kRevertSeconds * kFramesPerSecond);

	// 球体はまだ見せない（クローンが縮みきってから0から大きくする）
	worldTransform_.scale_ = {};
	UpdateWorldTransform(worldTransform_);

	// 落下はアニメーションが終わってから再開する
	throwVelocity_ = {};
	isThrown_ = false;
}

///// ----- 変形アニメーション ----- /////
// 素 → クローン：球体がスライムのように伸び縮み → 縮んで消える → クローンが0から現れる
// クローン → 素：クローンが縮んで消える → 球体が0から現れる
// ※アニメーション中はUpdate()側でreturnしているので、位置は動かず見た目だけが変化する
void CloneBase::UpdateTransformAnimation() {
	++animationTimer_;

	// アニメーション全体の進行度（0.0〜1.0）
	float progress = (animationDuration_ > 0) ? static_cast<float>(animationTimer_) / static_cast<float>(animationDuration_) : 1.0f;
	if (progress > 1.0f) {
		progress = 1.0f;
	}

	const bool isFinished = (animationTimer_ >= animationDuration_);

	if (state_ == State::kTransforming) {
		/// --- 素 → クローン ---
		// 球体が完全に消えるまでの区間（伸び縮み＋消える、をひとつながりで扱う）
		const float kBaseSpan = kTransformSquashRatio + kTransformShrinkRatio;
		
		if (progress < kBaseSpan) {
			// リンクがつながった瞬間から消えるまで、球体はずっと縮み続ける。
			// 最初にぐっと小さくなり、消える直前がゆっくりになる。
			float shrinkPhase = progress / kBaseSpan;
			float shrink = 1.0f - EaseOutSine(shrinkPhase);

			// 伸び縮み（スライムらしさ）は前半にだけ乗せる。
			// イージングをかけてから波にすることで、伸び縮みの速さに遊びが出る。
			float wave = 0.0f;
			if (progress < kTransformSquashRatio) {
				float phase = progress / kTransformSquashRatio;
				wave = std::sin(EaseInOutSine(phase) * std::numbers::pi_v<float> * 2.0f * kSquashWaveCount);
			}

			// 伸びる側だけ弱くして、縮んでいく流れを伸びが打ち消さないようにする
			float stretch = (wave >= 0.0f) ? wave * kStretchStrength : wave * kSquashStrength;

			worldTransform_.scale_.x = kBaseScale * shrink * (1.0f + stretch);
			worldTransform_.scale_.y = kBaseScale * shrink * (1.0f - stretch);
			worldTransform_.scale_.z = kBaseScale * shrink;

		} else {
			// 球体は完全に消し、クローンを0から本来の大きさまで大きくする
			worldTransform_.scale_ = {};

			float phase = (progress - kBaseSpan) / kTransformGrowRatio;
			if (phase > 1.0f) {
				phase = 1.0f;
			}

			cloneDrawScale_ = cloneModelScale_ * EaseOutBack(phase);
			if (cloneDrawScale_ < 0.0f) {
				cloneDrawScale_ = 0.0f;
			}
			player_->SetModelScaleImmediate(cloneDrawScale_);
		}

		UpdateWorldTransform(worldTransform_);

		if (isFinished) {
			// 球体は消したまま、クローンを本来の大きさに揃えて操作可能にする
			state_ = State::kTransformed;
			worldTransform_.scale_ = {};
			UpdateWorldTransform(worldTransform_);

			cloneDrawScale_ = cloneModelScale_;
			player_->SetModelScaleImmediate(cloneModelScale_);
		}
		return;
	}

	/// --- クローン → 素 ---
	if (progress < kRevertShrinkRatio) {
		// クローンを0まで縮めて消す
		float phase = progress / kRevertShrinkRatio;

		cloneDrawScale_ = cloneModelScale_ * (1.0f - EaseOutCubic(phase));
		player_->SetModelScaleImmediate(cloneDrawScale_);

		worldTransform_.scale_ = {};

	} else {
		// クローンは完全に消し、球体を0から本来の大きさまで大きくする
		cloneDrawScale_ = 0.0f;
		player_->SetModelScaleImmediate(0.0f);

		float phase = (progress - kRevertShrinkRatio) / kRevertGrowRatio;
		if (phase > 1.0f) {
			phase = 1.0f;
		}

		float scale = kBaseScale * EaseOutBack(phase);
		if (scale < 0.0f) {
			scale = 0.0f;
		}
		worldTransform_.scale_ = {scale, scale, scale};
	}

	UpdateWorldTransform(worldTransform_);

	if (isFinished) {
		state_ = State::kBase;
		worldTransform_.scale_ = {kBaseScale, kBaseScale, kBaseScale};
		UpdateWorldTransform(worldTransform_);

		// クローンの表示スケールは、次に変形する時のために元へ戻しておく
		cloneDrawScale_ = cloneModelScale_;
		player_->SetModelScaleImmediate(cloneModelScale_);

		// アニメーションが終わってから落下を再開する
		// （空中でリンクを切った場合は自然に落ち、地上なら着地判定でその場に留まる）
		throwVelocity_ = {};
		isThrown_ = true;
	}
}

///// ----- 投げられて飛んでいる間の物理更新 ----- /////
void CloneBase::UpdateThrowPhysics(const MapChipField::Rect& playerRect, const std::vector<MapChipField::Rect>& obstacleRects) {
	float halfWidth = kWidth / 2.0f;
	float halfHeight = kHeight / 2.0f;

	// 重力を加える（落下速度に上限を設ける）
	throwVelocity_.y = (std::max)(throwVelocity_.y - kThrowGravity, -kThrowMaxFallSpeed);

	Vector3 moveAmount = throwVelocity_;

	// ブロックとの当たり判定（上下左右）を行い、めり込まないように移動量を補正する。
	// X・Y軸をそれぞれ独立して判定するので、斜めに飛んでいてもブロックへめり込んだりすり抜けたりしない。
	BlockCollisionResult blockResult = CheckBlockCollision(moveAmount);

	// 他のクローンの素・クローン・自機・ドアなど、マップチップ以外の障害物との当たり判定。
	// ブロックで補正した後の移動量に対して、さらに補正をかける。
	BlockCollisionResult obstacleResult = CheckObstacleCollision(moveAmount, obstacleRects);
	blockResult.isCeilingHit = blockResult.isCeilingHit || obstacleResult.isCeilingHit;
	blockResult.isGroundHit = blockResult.isGroundHit || obstacleResult.isGroundHit;
	blockResult.isWallHit = blockResult.isWallHit || obstacleResult.isWallHit;

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
	ObjectColor* color = behavior_ == Behavior::kNormal ? nullptr : &chargeColor_;

	switch (state_) {
	case State::kTransformed:
		player_->Draw(color);
		break;

	case State::kBase:
		if (modelBase_ != nullptr) {
			modelBase_->Draw(worldTransform_, *camera_, color);
		}
		break;

	case State::kTransforming:
	case State::kReverting:
		// アニメーション中は、消えかけの球体と現れかけのクローンを両方描く。
		// 片方は必ずスケール0になっているので、実際に見えるのはどちらか一方だけになる。
		if (modelBase_ != nullptr) {
			modelBase_->Draw(worldTransform_, *camera_, color);
		}
		player_->Draw(color);
		break;
	}

}

///// ----- マップチップ以外の障害物との当たり判定 ----- /////
// 他のクローンの素・クローン・自機・ドアなど、矩形で表される障害物にめり込まないよう移動量を補正する。
CloneBase::BlockCollisionResult CloneBase::CheckObstacleCollision(Vector3& moveAmount, const std::vector<MapChipField::Rect>& obstacleRects) const {
	BlockCollisionResult result;

	// 先に横方向を確定させ、そのあと「横に動いた後の位置」で縦方向を判定する。
	// この順番にしないと、斜めに飛んだ時にどちらの軸でも「まだ重なっていない」と判定されて、
	// 相手の角から中へ入り込めてしまう。
	CheckObstacleCollisionRight(moveAmount, result, obstacleRects);
	CheckObstacleCollisionLeft(moveAmount, result, obstacleRects);
	CheckObstacleCollisionTop(moveAmount, result, obstacleRects);
	CheckObstacleCollisionBottom(moveAmount, result, obstacleRects);

	return result;
}

/// --- 右方向 ---
void CloneBase::CheckObstacleCollisionRight(Vector3& moveAmount, BlockCollisionResult& result, const std::vector<MapChipField::Rect>& obstacleRects) const {
	if (moveAmount.x <= 0.0f) {
		return;
	}

	float halfWidth = kWidth / 2.0f;
	float halfHeight = kHeight / 2.0f;
	float nowBottom = worldTransform_.translation_.y - halfHeight;
	float nowTop = worldTransform_.translation_.y + halfHeight;
	float nowRight = worldTransform_.translation_.x + halfWidth;

	for (const MapChipField::Rect& rect : obstacleRects) {
		// 縦方向が重なっていない障害物は無視する
		if (nowTop <= rect.bottom || nowBottom >= rect.top) {
			continue;
		}

		float newRight = nowRight + moveAmount.x;
		if (nowRight <= rect.left && newRight > rect.left) {
			moveAmount.x = rect.left - nowRight - kBlank;
			result.isWallHit = true;
		}
	}
}

/// --- 左方向 ---
void CloneBase::CheckObstacleCollisionLeft(Vector3& moveAmount, BlockCollisionResult& result, const std::vector<MapChipField::Rect>& obstacleRects) const {
	if (moveAmount.x >= 0.0f) {
		return;
	}

	float halfWidth = kWidth / 2.0f;
	float halfHeight = kHeight / 2.0f;
	float nowBottom = worldTransform_.translation_.y - halfHeight;
	float nowTop = worldTransform_.translation_.y + halfHeight;
	float nowLeft = worldTransform_.translation_.x - halfWidth;

	for (const MapChipField::Rect& rect : obstacleRects) {
		// 縦方向が重なっていない障害物は無視する
		if (nowTop <= rect.bottom || nowBottom >= rect.top) {
			continue;
		}

		float newLeft = nowLeft + moveAmount.x;
		if (nowLeft >= rect.right && newLeft < rect.right) {
			moveAmount.x = rect.right - nowLeft + kBlank;
			result.isWallHit = true;
		}
	}
}

/// --- 上方向 ---
void CloneBase::CheckObstacleCollisionTop(Vector3& moveAmount, BlockCollisionResult& result, const std::vector<MapChipField::Rect>& obstacleRects) const {
	if (moveAmount.y <= 0.0f) {
		return;
	}

	float halfWidth = kWidth / 2.0f;
	float halfHeight = kHeight / 2.0f;
	// 横方向はすでに確定しているので、「横に動いた後」のX座標で重なりを判定する
	float movedX = worldTransform_.translation_.x + moveAmount.x;
	float nowLeft = movedX - halfWidth;
	float nowRight = movedX + halfWidth;
	float nowTop = worldTransform_.translation_.y + halfHeight;

	for (const MapChipField::Rect& rect : obstacleRects) {
		// 横方向が重なっていない障害物は無視する
		if (nowRight <= rect.left || nowLeft >= rect.right) {
			continue;
		}

		float newTop = nowTop + moveAmount.y;
		if (nowTop <= rect.bottom && newTop > rect.bottom) {
			moveAmount.y = rect.bottom - nowTop - kBlank;
			result.isCeilingHit = true;
		}
	}
}

/// --- 下方向 ---
void CloneBase::CheckObstacleCollisionBottom(Vector3& moveAmount, BlockCollisionResult& result, const std::vector<MapChipField::Rect>& obstacleRects) const {
	if (moveAmount.y >= 0.0f) {
		return;
	}

	float halfWidth = kWidth / 2.0f;
	float halfHeight = kHeight / 2.0f;
	// 横方向はすでに確定しているので、「横に動いた後」のX座標で重なりを判定する
	float movedX = worldTransform_.translation_.x + moveAmount.x;
	float nowLeft = movedX - halfWidth;
	float nowRight = movedX + halfWidth;
	float nowBottom = worldTransform_.translation_.y - halfHeight;

	for (const MapChipField::Rect& rect : obstacleRects) {
		// 横方向が重なっていない障害物は無視する
		if (nowRight <= rect.left || nowLeft >= rect.right) {
			continue;
		}

		float newBottom = nowBottom + moveAmount.y;
		if (nowBottom >= rect.top && newBottom < rect.top) {
			moveAmount.y = rect.top - nowBottom + kBlank;
			result.isGroundHit = true;
		}
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

void CloneBase::Charge() {
	const bool wasCharged = isCharged_;

	isCharged_ = true;
	hasBeenCharged_ = true;
	chargeTimer_ = static_cast<int>(chargeDurationSeconds_ * kFramesPerSecond);

	// レーザーへ触れている間は毎フレームChargeされるため、
	// 初めて帯電した瞬間だけBehaviorを切り替える
	if (!wasCharged) {
		behaviorRequest_ = Behavior::kCharging;
	}
}

void CloneBase::Discharge() {
	if (!isCharged_) {
		return;
	}

	isCharged_ = false;
	chargeTimer_ = 0;
	behaviorRequest_ = Behavior::kDischarge;
}

void CloneBase::UpdateBehavior() {
	if (behaviorRequest_) {
		behavior_ = behaviorRequest_.value();

		switch (behavior_) {
		case Behavior::kNormal:
			BehaviorNormalInitialize();
			break;

		case Behavior::kCharging:
			BehaviorChargingInitialize();
			break;

		case Behavior::kDischarge:
			BehaviorDischargeInitialize();
			break;
		}

		behaviorRequest_ = std::nullopt;
	}

	switch (behavior_) {
	case Behavior::kNormal:
		BehaviorNormalUpdate();
		break;

	case Behavior::kCharging:
		BehaviorChargingUpdate();
		break;

	case Behavior::kDischarge:
		BehaviorDischargeUpdate();
		break;
	}
}

void CloneBase::BehaviorNormalInitialize() {
	behaviorTimer_ = 0.0f;
	chargeEffectTime_ = 0.0f;
}

void CloneBase::BehaviorNormalUpdate() {
	// 通常状態
}

void CloneBase::BehaviorChargingInitialize() {
	behaviorTimer_ = 0.0f;
	chargeEffectTime_ = 0.0f;
}

void CloneBase::BehaviorChargingUpdate() {
	chargeEffectTime_ += 1.0f / 60.0f;

	const float remainingSeconds = GetChargeRemainingSeconds();

	if (remainingSeconds <= 1.0f) {
		const bool flash = static_cast<int>(chargeEffectTime_ * 12.0f) % 2 == 0;

		if (flash) {
			chargeColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		} else {
			chargeColor_.SetColor({0.1f, 0.5f, 1.0f, 1.0f});
		}
	} else {
		const float pulse = (std::sin(chargeEffectTime_ * 6.0f) + 1.0f) * 0.5f;

		chargeColor_.SetColor({
		    0.1f + pulse * 0.2f,
		    0.5f + pulse * 0.3f,
		    1.0f,
		    1.0f,
		});
	}

	if (!isCharged_) {
		behaviorRequest_ = Behavior::kDischarge;
	}
}

void CloneBase::BehaviorDischargeInitialize() { behaviorTimer_ = 0.0f; }

void CloneBase::BehaviorDischargeUpdate() {
	behaviorTimer_ += 1.0f / 60.0f;

	const bool flash = static_cast<int>(behaviorTimer_ * 20.0f) % 2 == 0;

	if (flash) {
		chargeColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
	} else {
		chargeColor_.SetColor({0.1f, 0.5f, 1.0f, 1.0f});
	}

	if (behaviorTimer_ >= kDischargeEffectDuration) {
		behaviorRequest_ = Behavior::kNormal;
	}
}
