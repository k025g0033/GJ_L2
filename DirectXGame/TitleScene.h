#pragma once
#include "IScene.h"
#include "KamataEngine.h"

// タイトルシーン
class TitleScene : public IScene {
public:
	~TitleScene() override;

	void Initialize() override;
	void Update() override;
	void Draw() override;

	bool IsFinished() const override { return isFinished_; }

private:
	enum class MenuItem {
		kStart,
		kExit,
	};

	bool isFinished_ = false;
	MenuItem selectedItem_ = MenuItem::kStart;
	uint32_t startTextureHandle_ = 0;
	uint32_t exitTextureHandle_ = 0;
	uint32_t cursorMoveSoundHandle_ = 0;
	uint32_t decideSoundHandle_ = 0;
	KamataEngine::Sprite* startSprite_ = nullptr;
	KamataEngine::Sprite* exitSprite_ = nullptr;
	KamataEngine::Model* titleModel_ = nullptr;
	KamataEngine::WorldTransform titleWorldTransform_;
	KamataEngine::Camera titleCamera_;
	float selectionAnimationTime_ = 0.0f;

	static inline const float kImageSize = 128.0f;
	static inline const float kAnimationScale = 0.12f;
	static inline const float kAnimationSpeed = 4.0f;
};
