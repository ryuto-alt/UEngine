#include "EndingScene.h"
#include "SceneManager.h"
#include "Audio/AudioManager.h"

void EndingScene::Initialize() {
    OutputDebugStringA("EndingScene::Initialize - START\n");

    camera_->SetTranslate({0.0f, 0.0f, -10.0f});

    vhsEffect_ = std::make_unique<PostProcess>();
    vhsEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::VHS);

    // Black background
    bgSprite_ = std::make_unique<Sprite>();
    bgSprite_->Initialize(spriteCommon_, "Resources/textures/white1x1.png");
    bgSprite_->SetPosition({0.0f, 0.0f});
    bgSprite_->SetSize({1280.0f, 720.0f});
    bgSprite_->setColor({0.0f, 0.0f, 0.0f, 1.0f});

    // Asa image — darkened background, stays visible during text scroll
    asaSprite_ = std::make_unique<Sprite>();
    asaSprite_->Initialize(spriteCommon_, "Resources/textures/Ending/asa.png");
    asaSprite_->SetPosition({0.0f, 0.0f});
    asaSprite_->SetSize({1280.0f, 720.0f});
    asaSprite_->setColor({0.18f, 0.18f, 0.2f, 1.0f});

    // Ending text image
    textSprite_ = std::make_unique<Sprite>();
    textSprite_->Initialize(spriteCommon_, "Resources/textures/Ending/ending_text.png");
    textSprite_->SetAnchorPoint({0.5f, 0.0f});

    // Full-screen fade sprite (black overlay)
    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(spriteCommon_, "Resources/textures/white1x1.png");
    fadeSprite_->SetPosition({0.0f, 0.0f});
    fadeSprite_->SetSize({1280.0f, 720.0f});
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, 1.0f});

    scrollY_ = kStartY;
    fadeAlpha_ = 1.0f;
    bgmVolume_ = 0.2f;

    // Load audio
    auto* audio = AudioManager::GetInstance();
    audio->LoadMP3("endingBell", "Resources/Audio/mezamasi/bell.mp3");
    audio->LoadMP3("endingStop", "Resources/Audio/mezamasi/stop.mp3");
    audio->LoadMP3("endingBGM", "Resources/Audio/song/ep.mp3");

    // Start bell immediately
    phase_ = Phase::Bell;
    phaseTimer_ = 0.0f;
    audio->Play("endingBell", false);

    OutputDebugStringA("EndingScene::Initialize - DONE\n");
}

void EndingScene::Update() {
    camera_->Update();

    constexpr float kDeltaTime = 1.0f / 60.0f;
    timer_ += kDeltaTime;
    phaseTimer_ += kDeltaTime;

    auto* audio = AudioManager::GetInstance();

    // Skip with Space or Enter
    if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
        audio->Stop("endingBell");
        audio->Stop("endingStop");
        audio->Stop("endingBGM");
        sceneManager_->ChangeScene("GameClear");
        return;
    }

    switch (phase_) {
    case Phase::Bell:
        if (phaseTimer_ >= kBellDuration) {
            audio->Stop("endingBell");
            audio->Play("endingStop", false);
            phase_ = Phase::AlarmStop;
            phaseTimer_ = 0.0f;
        }
        break;

    case Phase::AlarmStop:
        if (!audio->IsPlaying("endingStop")) {
            phase_ = Phase::AsaFadeIn;
            phaseTimer_ = 0.0f;
        }
        break;

    case Phase::AsaFadeIn:
        // Black overlay fades out slowly, revealing darkened asa.png
        fadeAlpha_ = 1.0f - phaseTimer_ / kAsaFadeInDuration;
        if (fadeAlpha_ <= 0.0f) {
            fadeAlpha_ = 0.0f;
            // Start BGM + text scroll
            audio->SetVolume("endingBGM", 0.2f);
            audio->Play("endingBGM", false);
            phase_ = Phase::Scrolling;
            phaseTimer_ = 0.0f;
        }
        break;

    case Phase::Scrolling:
        // asa.png stays visible as background, text scrolls on top
        scrollY_ -= kScrollSpeed * kDeltaTime;
        if (scrollY_ <= kEndY) {
            phase_ = Phase::FadeOut;
            phaseTimer_ = 0.0f;
            fadeAlpha_ = 0.0f;
        }
        break;

    case Phase::FadeOut:
        fadeAlpha_ += kDeltaTime / kFadeOutDuration;
        bgmVolume_ = 0.2f * (1.0f - phaseTimer_ / kBgmFadeOutDuration);
        if (bgmVolume_ < 0.0f) bgmVolume_ = 0.0f;
        audio->SetVolume("endingBGM", bgmVolume_);

        if (fadeAlpha_ >= 1.0f) {
            fadeAlpha_ = 1.0f;
            audio->Stop("endingBGM");
            sceneManager_->ChangeScene("GameClear");
            return;
        }
        break;
    }

    // Update sprites
    textSprite_->SetPosition({640.0f, scrollY_});
    bgSprite_->Update();
    asaSprite_->Update();
    textSprite_->Update();
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, fadeAlpha_});
    fadeSprite_->Update();

    // VHS: heavier grain/noise for asa background feel, lighter during text
    bool asaVisible = (phase_ == Phase::AsaFadeIn || phase_ == Phase::Scrolling || phase_ == Phase::FadeOut);
    if (asaVisible) {
        vhsEffect_->SetVHSParams(
            timer_,
            0.20f,  // scanlineIntensity — stronger
            0.12f,  // noiseIntensity — grainy
            0.15f,  // trackingError
            0.4f,   // chromaticAberration
            0.25f,  // colorBleed
            0.4f,   // sharpness — lower = rougher
            0.10f   // tapeCrease
        );
    } else {
        vhsEffect_->SetVHSParams(
            timer_,
            0.08f, 0.03f, 0.1f, 0.3f, 0.15f, 0.7f, 0.05f
        );
    }
}

void EndingScene::Draw() {
    vhsEffect_->PreDraw();

    spriteCommon_->CommonDraw();
    bgSprite_->Draw();

    // asa.png background (visible from AsaFadeIn through FadeOut)
    bool asaVisible = (phase_ == Phase::AsaFadeIn || phase_ == Phase::Scrolling || phase_ == Phase::FadeOut);
    if (asaVisible) {
        asaSprite_->Draw();
    }

    // Text (visible from Scrolling through FadeOut)
    if (phase_ == Phase::Scrolling || phase_ == Phase::FadeOut) {
        textSprite_->Draw();
    }

    // Black fade overlay
    if (fadeAlpha_ > 0.0f) {
        spriteCommon_->CommonDraw();
        fadeSprite_->Draw();
    }

    vhsEffect_->PostDraw();
}

void EndingScene::Finalize() {
    auto* audio = AudioManager::GetInstance();
    audio->Stop("endingBell");
    audio->Stop("endingStop");
    audio->Stop("endingBGM");

    bgSprite_.reset();
    asaSprite_.reset();
    textSprite_.reset();
    fadeSprite_.reset();
    if (vhsEffect_) {
        vhsEffect_->Finalize();
        vhsEffect_.reset();
    }
}
