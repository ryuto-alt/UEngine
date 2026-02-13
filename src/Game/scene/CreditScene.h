#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
#include "Audio/AudioManager.h"
#include <memory>

class CreditScene : public IScene {
public:
    CreditScene() = default;
    ~CreditScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Sprite> bgSprite_;
    std::unique_ptr<Sprite> creditTextSprite_;
    std::unique_ptr<Sprite> fadeSprite_;
    std::unique_ptr<PostProcess> vignetteEffect_;

    float scrollY_ = 0.0f;
    float timer_ = 0.0f;
    float phaseTimer_ = 0.0f;
    float fadeAlpha_ = 1.0f;
    bool bgmLoaded_ = false;

    enum class Phase { FadeIn, Scrolling, FadeOut };
    Phase phase_ = Phase::FadeIn;

    static constexpr float kScrollSpeed = 30.0f;
    static constexpr float kTextHeight = 3000.0f;
    static constexpr float kStartY = 720.0f;
    static constexpr float kEndY = -kTextHeight;
    static constexpr float kFadeInDuration = 2.0f;
    static constexpr float kFadeOutDuration = 4.0f;
};
