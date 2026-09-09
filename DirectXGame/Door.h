#pragma once

#include "KamataEngine.h"
#include "MapChipField.h"
#include "Player.h"
#include <cstdint>

// 同じIDの鍵から開閉状態を受け取り、閉／開のモデルを差し替える扉
class Door {
public:
	void Initialize(
	    KamataEngine::Model* modelClosed, KamataEngine::Model* modelOpen, KamataEngine::Model* modelOpenGlass, KamataEngine::Camera* camera, const KamataEngine::Vector3& position, uint8_t id);
	void Update();
	void Draw();

	void SetOpen(bool isOpen);
	bool IsOpen() const { return isOpen_; }
	uint8_t GetID() const { return id_; }
	MapChipField::Rect GetRect() const;

	bool IsCollidingWithPlayer(const Player* player) const;

private:
	static inline const float kCollisionCenterOffsetY = 0.5f;

	KamataEngine::Vector3 closedPosition_{};

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor color_;
	// 閉じている間と開いた後で、描画するモデルだけを差し替える
	KamataEngine::Model* modelClosed_ = nullptr;
	KamataEngine::Model* modelOpen_ = nullptr;
	// 開いた扉の板ガラス。1メッシュにマテリアルは1つしか持てないので別モデルにしてある。
	KamataEngine::Model* modelOpenGlass_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	uint8_t id_ = 0;
	bool isOpen_ = false;
	// 色はDoor.png／Door_open.pngをそのまま使用する。
	static inline const KamataEngine::Vector4 kClosedColor = {1.0f, 1.0f, 1.0f, 1.0f};
};
