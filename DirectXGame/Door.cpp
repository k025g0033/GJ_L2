#include "Door.h"
#include "WorldTransformConfig.h"
#include <numbers>

using namespace KamataEngine;

///// ----- 初期化 ----- /////

void Door::Initialize(Model* modelClosed, Model* modelOpen, Model* modelOpenGlass, Camera* camera, const Vector3& position, uint8_t id) {
	modelClosed_ = modelClosed;
	modelOpen_ = modelOpen;
	modelOpenGlass_ = modelOpenGlass;
	camera_ = camera;
	id_ = id;

	// OBJの原点は底面にあるので、CSVマス中心から半マス下げて床上面へ合わせる。
	closedPosition_ = position;
	closedPosition_.y -= MapChipField::kBlockHeight / 2.0f;

	worldTransform_.Initialize();
	// Blenderモデルは約2マス角で横幅がZ方向なので、1マスへ縮小して正面をカメラへ向ける。
	worldTransform_.scale_ = {0.5f, 0.5f, 0.5f};
	worldTransform_.rotation_.y = -std::numbers::pi_v<float> / 2.0f;
	worldTransform_.translation_ = closedPosition_;
	UpdateWorldTransform(worldTransform_);

	color_.Initialize();
	color_.SetColor(kClosedColor);
}

///// ----- 更新 ----- /////

void Door::Update() { UpdateWorldTransform(worldTransform_); }

void Door::SetOpen(bool isOpen) {
	// 動きは付けず、状態を切り替えるだけにする。
	isOpen_ = isOpen;
}

///// ----- 描画 ----- /////

void Door::Draw() {
	/// --- 閉じている間は通常モデルだけ ---
	if (!isOpen_) {
		if (modelClosed_ != nullptr) {
			modelClosed_->Draw(worldTransform_, *camera_, &color_);
		}
		return;
	}

	/// --- 開いた後は扉本体と板ガラスを同じ姿勢で重ねる ---
	if (modelOpen_ != nullptr) {
		modelOpen_->Draw(worldTransform_, *camera_, &color_);
	}
	if (modelOpenGlass_ != nullptr) {
		modelOpenGlass_->Draw(worldTransform_, *camera_, &color_);
	}
}

///// ----- 当たり判定 ----- /////

MapChipField::Rect Door::GetRect() const {
	const Vector3& position = closedPosition_;
	const float centerY = position.y + kCollisionCenterOffsetY;
	return {
	    position.x - MapChipField::kBlockWidth / 2.0f,
	    position.x + MapChipField::kBlockWidth / 2.0f,
	    centerY - MapChipField::kBlockHeight / 2.0f,
	    centerY + MapChipField::kBlockHeight / 2.0f,
	};
}

bool Door::IsCollidingWithPlayer(const Player* player) const {
	MapChipField::Rect rect = {
	    closedPosition_.x - MapChipField::kBlockWidth / 2.0f,
	    closedPosition_.x + MapChipField::kBlockWidth / 2.0f,
	    closedPosition_.y + kCollisionCenterOffsetY - MapChipField::kBlockHeight / 2.0f,
	    closedPosition_.y + kCollisionCenterOffsetY + MapChipField::kBlockHeight / 2.0f,
	};

	const Vector3& position = player->GetWorldTransform().translation_;

	return position.x + player->GetWidth() / 2.0f > rect.left && position.x - player->GetWidth() / 2.0f < rect.right && position.y + player->GetHeight() / 2.0f > rect.bottom &&
	       position.y - player->GetHeight() / 2.0f < rect.top;
}
