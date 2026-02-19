#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
#include "../UI/SettingsMenu.h"
#include <memory>

class IntroScene : public IScene {
public:
    IntroScene() = default;
    ~IntroScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    enum class Phase {
        FadeIn,
        Scrolling,
        FadeOut
    };

    std::unique_ptr<Sprite> bgSprite_;
    std::unique_ptr<Sprite> textSprite_;
    std::unique_ptr<Sprite> fadeSprite_;
    std::unique_ptr<PostProcess> vignetteEffect_;
    std::unique_ptr<PostProcess> crtEffect_;

    Phase phase_ = Phase::FadeIn;
    float scrollY_ = 0.0f;
    float timer_ = 0.0f;
    float phaseTimer_ = 0.0f;
    float fadeAlpha_ = 1.0f;
    float bgAlpha_ = 0.0f;   // 背景フェードイン用（中盤から徐々に表示）
    float bgmVolume_ = 0.0f;

    static constexpr float kScrollSpeed = 20.0f;
    static constexpr float kTextHeight = 3000.0f;
    static constexpr float kStartY = 720.0f;
    static constexpr float kEndY = -kTextHeight;

    static constexpr float kFadeInDuration = 2.0f;
    static constexpr float kFadeOutDuration = 3.0f;
    static constexpr float kBgmMaxVolume = 0.15f;
    static constexpr float kBgmFadeInDuration = 2.0f;
    static constexpr float kBgmFadeOutDuration = 3.0f;

    // 設定メニュー
    std::unique_ptr<SettingsMenu> settingsMenu_;
};
