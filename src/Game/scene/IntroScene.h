#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
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
    std::unique_ptr<Sprite> bgSprite_;
    std::unique_ptr<Sprite> textSprite_;
    std::unique_ptr<PostProcess> vignetteEffect_;

    float scrollY_ = 0.0f;
    float timer_ = 0.0f;

    static constexpr float kScrollSpeed = 30.0f;
    static constexpr float kTextHeight = 3000.0f;
    static constexpr float kStartY = 720.0f;
    static constexpr float kEndY = -kTextHeight;
};
