#include "CreditScene.h"
#include "SceneManager.h"

void CreditScene::Initialize() {
    camera_->SetTranslate({0.0f, 0.0f, -10.0f});

    // Vignette only (Horror effect with everything else at 0)
    vignetteEffect_ = std::make_unique<PostProcess>();
    vignetteEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::Horror);

    crtEffect_ = std::make_unique<PostProcess>();
    crtEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::CRT);

    // Black background
    bgSprite_ = std::make_unique<Sprite>();
    bgSprite_->Initialize(spriteCommon_, "Resources/textures/common/white1x1.png");
    bgSprite_->SetPosition({0.0f, 0.0f});
    bgSprite_->SetSize({1280.0f, 720.0f});
    bgSprite_->setColor({0.0f, 0.0f, 0.0f, 1.0f});

    // Credit text image (scrolls up)
    creditTextSprite_ = std::make_unique<Sprite>();
    creditTextSprite_->Initialize(spriteCommon_, "Resources/textures/Ending/credit_text.png");
    creditTextSprite_->SetAnchorPoint({0.5f, 0.0f});

    // Fade overlay
    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(spriteCommon_, "Resources/textures/common/white1x1.png");
    fadeSprite_->SetPosition({0.0f, 0.0f});
    fadeSprite_->SetSize({1280.0f, 720.0f});
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, 1.0f});

    scrollY_ = kStartY;
    fadeAlpha_ = 1.0f;
    timer_ = 0.0f;
    phaseTimer_ = 0.0f;
    phase_ = Phase::FadeIn;

    // Load and play credit BGM
    auto* audio = AudioManager::GetInstance();
    bgmLoaded_ = audio->LoadMP3("creditBGM", "Resources/audio/bgm/credit.mp3");
    if (bgmLoaded_) {
        audio->SetVolume("creditBGM", 0.15f);
        audio->Play("creditBGM", false);
    }
}

void CreditScene::Update() {
    camera_->Update();

    // PostProcess resize on window size change
    {
        static uint32_t prevW = 0, prevH = 0;
        uint32_t curW = dxCommon_->GetCurrentWindowWidth();
        uint32_t curH = dxCommon_->GetCurrentWindowHeight();
        if (prevW != curW || prevH != curH) {
            if (prevW != 0) {
                if (vignetteEffect_) vignetteEffect_->ResizeRenderTarget();
                if (crtEffect_) crtEffect_->ResizeRenderTarget();
            }
            prevW = curW;
            prevH = curH;
        }
    }

    constexpr float kDeltaTime = 1.0f / 60.0f;
    timer_ += kDeltaTime;
    phaseTimer_ += kDeltaTime;

    auto* audio = AudioManager::GetInstance();

    // Skip with Space
    if (input_->TriggerKey(DIK_SPACE)) {
        if (bgmLoaded_) {
            audio->Stop("creditBGM");
        }
        sceneManager_->ChangeScene("Title");
        return;
    }

    switch (phase_) {
    case Phase::FadeIn:
        fadeAlpha_ = 1.0f - phaseTimer_ / kFadeInDuration;
        if (fadeAlpha_ <= 0.0f) {
            fadeAlpha_ = 0.0f;
            phase_ = Phase::Scrolling;
            phaseTimer_ = 0.0f;
        }
        break;

    case Phase::Scrolling:
        scrollY_ -= kScrollSpeed * kDeltaTime;
        if (scrollY_ <= kEndY) {
            phase_ = Phase::FadeOut;
            phaseTimer_ = 0.0f;
            fadeAlpha_ = 0.0f;
        }
        break;

    case Phase::FadeOut:
        fadeAlpha_ = phaseTimer_ / kFadeOutDuration;
        // Fade out BGM
        if (bgmLoaded_) {
            float vol = 0.15f * (1.0f - phaseTimer_ / kFadeOutDuration);
            if (vol < 0.0f) vol = 0.0f;
            audio->SetVolume("creditBGM", vol);
        }
        if (fadeAlpha_ >= 1.0f) {
            fadeAlpha_ = 1.0f;
            if (bgmLoaded_) {
                audio->Stop("creditBGM");
            }
            sceneManager_->ChangeScene("Title");
            return;
        }
        break;
    }

    // Update sprites
    creditTextSprite_->SetPosition({640.0f, scrollY_});
    bgSprite_->Update();
    creditTextSprite_->Update();
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, fadeAlpha_});
    fadeSprite_->Update();

    // Vignette effect only (no noise, no distortion, no blood)
    vignetteEffect_->SetHorrorParams(timer_, 0.0f, 0.0f, 0.0f, 0.8f);

    // CRTブラウン管エフェクトのパラメータ更新
    if (crtEffect_) {
        float aspect = static_cast<float>(dxCommon_->GetCurrentWindowWidth())
                     / static_cast<float>(dxCommon_->GetCurrentWindowHeight());
        crtEffect_->SetCRTParams(0.06f, 0.08f, 0.30f, 0.008f, aspect, 4.0f / 3.0f);
    }
}

void CreditScene::Draw() {
    vignetteEffect_->PreDraw();

    spriteCommon_->CommonDraw();
    bgSprite_->Draw();
    creditTextSprite_->Draw();

    // Fade overlay
    if (fadeAlpha_ > 0.0f) {
        spriteCommon_->CommonDraw();
        fadeSprite_->Draw();
    }

    if (crtEffect_) {
        vignetteEffect_->PostDrawTo(crtEffect_.get());
        crtEffect_->PostDraw();
    } else {
        vignetteEffect_->PostDraw();
    }
}

void CreditScene::Finalize() {
    auto* audio = AudioManager::GetInstance();
    audio->Stop("creditBGM");

    bgSprite_.reset();
    creditTextSprite_.reset();
    fadeSprite_.reset();
    if (vignetteEffect_) {
        vignetteEffect_->Finalize();
        vignetteEffect_.reset();
    }
    if (crtEffect_) {
        crtEffect_->Finalize();
        crtEffect_.reset();
    }
}
