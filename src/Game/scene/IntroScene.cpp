#include "IntroScene.h"
#include "SceneManager.h"

void IntroScene::Initialize() {
    camera_->SetTranslate({0.0f, 0.0f, -10.0f});

    vignetteEffect_ = std::make_unique<PostProcess>();
    vignetteEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::VHS);

    bgSprite_ = std::make_unique<Sprite>();
    bgSprite_->Initialize(spriteCommon_, "Resources/textures/Intro/intro_bg.png");
    bgSprite_->SetPosition({0.0f, 0.0f});
    bgSprite_->SetSize({1280.0f, 720.0f});

    textSprite_ = std::make_unique<Sprite>();
    textSprite_->Initialize(spriteCommon_, "Resources/textures/Intro/intro_text.png");
    textSprite_->SetAnchorPoint({0.5f, 0.0f});

    scrollY_ = kStartY;
}

void IntroScene::Update() {
    camera_->Update();

    constexpr float kDeltaTime = 1.0f / 60.0f;
    timer_ += kDeltaTime;

    if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
        sceneManager_->ChangeScene("GamePlay");
        return;
    }

    scrollY_ -= kScrollSpeed * kDeltaTime;

    if (scrollY_ <= kEndY) {
        sceneManager_->ChangeScene("GamePlay");
        return;
    }

    textSprite_->SetPosition({640.0f, scrollY_});

    bgSprite_->Update();
    textSprite_->Update();

    vignetteEffect_->SetVHSParams(
        timer_,
        0.15f, // scanlineIntensity
        0.08f, // noiseIntensity
        0.3f,  // trackingError
        0.8f,  // chromaticAberration
        0.3f,  // colorBleed
        0.3f,  // sharpness
        0.15f  // tapeCrease
    );
}

void IntroScene::Draw() {
    vignetteEffect_->PreDraw();

    spriteCommon_->CommonDraw();
    bgSprite_->Draw();
    textSprite_->Draw();

    vignetteEffect_->PostDraw();
}

void IntroScene::Finalize() {
    if (bgSprite_) {
        bgSprite_.reset();
    }
    if (textSprite_) {
        textSprite_.reset();
    }
    if (vignetteEffect_) {
        vignetteEffect_->Finalize();
        vignetteEffect_.reset();
    }
}
