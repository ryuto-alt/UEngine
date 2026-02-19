#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
#include "../UI/SettingsMenu.h"
#include <memory>

class EndingScene : public IScene {
public:
    EndingScene() = default;
    ~EndingScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Sprite> bgSprite_;
    std::unique_ptr<Sprite> asaSprite_;
    std::unique_ptr<Sprite> textSprite_;
    std::unique_ptr<Sprite> fadeSprite_;
    std::unique_ptr<PostProcess> vhsEffect_;
    std::unique_ptr<PostProcess> crtEffect_;

    float scrollY_ = 0.0f;
    float timer_ = 0.0f;
    float phaseTimer_ = 0.0f;
    float fadeAlpha_ = 1.0f;
    float bgmVolume_ = 0.05f;
    bool skipRequested_ = false;

    enum class Phase { Bell, AlarmStop, AsaFadeIn, Scrolling, FadeOut };
    Phase phase_ = Phase::Bell;

    static constexpr float kScrollSpeed = 30.0f;
    static constexpr float kTextHeight = 3000.0f;
    static constexpr float kStartY = 720.0f;
    static constexpr float kEndY = -kTextHeight;
    static constexpr float kBellDuration = 3.0f;
    static constexpr float kAsaFadeInDuration = 4.0f;
    static constexpr float kFadeOutDuration = 4.0f;
    static constexpr float kBgmFadeOutDuration = 4.0f;

    // 設定メニュー
    std::unique_ptr<SettingsMenu> settingsMenu_;
};
