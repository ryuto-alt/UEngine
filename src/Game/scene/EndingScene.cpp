#include "EndingScene.h"
#include "SceneManager.h"
#include "Audio/AudioManager.h"

void EndingScene::Initialize() {
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
    bgmVolume_ = 0.05f;
    skipRequested_ = false;
    timer_ = 0.0f;
    phaseTimer_ = 0.0f;

    // Load audio
    auto* audio = AudioManager::GetInstance();
    audio->LoadMP3("endingBell", "Resources/Audio/mezamasi/bell.mp3");
    audio->LoadMP3("endingStop", "Resources/Audio/mezamasi/stop.mp3");
    audio->LoadMP3("endingBGM", "Resources/Audio/song/ep.mp3");

    // Start bell immediately
    phase_ = Phase::Bell;
    phaseTimer_ = 0.0f;
    audio->SetVolume("endingBell", 0.25f);
    audio->Play("endingBell", false);

    // 設定メニューの初期化
    settingsMenu_ = std::make_unique<SettingsMenu>();
    settingsMenu_->Initialize(spriteCommon_, input_);
}

void EndingScene::Update() {
    camera_->Update();

    // PostProcess resize on window size change
    {
        static uint32_t prevW = 0, prevH = 0;
        uint32_t curW = dxCommon_->GetCurrentWindowWidth();
        uint32_t curH = dxCommon_->GetCurrentWindowHeight();
        if (prevW != curW || prevH != curH) {
            if (prevW != 0 && vhsEffect_) {
                vhsEffect_->ResizeRenderTarget();
            }
            prevW = curW;
            prevH = curH;
        }
    }

    constexpr float kDeltaTime = 1.0f / 60.0f;

    // ESC key: toggle settings menu
    if (input_->TriggerKey(DIK_ESCAPE) && settingsMenu_) {
        if (settingsMenu_->IsOpen()) {
            settingsMenu_->Close();
        } else {
            settingsMenu_->Open();
        }
    }

    // 設定メニューが開いている間はシーン更新をスキップ
    if (settingsMenu_ && settingsMenu_->IsOpen()) {
        settingsMenu_->Update(kDeltaTime);
        return;
    }
    timer_ += kDeltaTime;
    phaseTimer_ += kDeltaTime;

    auto* audio = AudioManager::GetInstance();

    // Skip with Space - fade out gracefully
    if (input_->TriggerKey(DIK_SPACE)) {
        audio->Stop("endingBell");
        audio->Stop("endingStop");
        audio->Stop("endingBGM");

        skipRequested_ = true;
        phase_ = Phase::FadeOut;
        phaseTimer_ = 0.0f;
        fadeAlpha_ = 0.0f;
    }

    switch (phase_) {
    case Phase::Bell:
        if (phaseTimer_ >= kBellDuration) {
            audio->Stop("endingBell");
            audio->SetVolume("endingStop", 0.25f);
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
        fadeAlpha_ = 1.0f - phaseTimer_ / kAsaFadeInDuration;
        if (fadeAlpha_ <= 0.0f) {
            fadeAlpha_ = 0.0f;
            audio->SetVolume("endingBGM", 0.15f);
            audio->Play("endingBGM", false);
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
        fadeAlpha_ += kDeltaTime / kFadeOutDuration;
        if (!skipRequested_) {
            bgmVolume_ = 0.15f * (1.0f - phaseTimer_ / kBgmFadeOutDuration);
            if (bgmVolume_ < 0.0f) bgmVolume_ = 0.0f;
            audio->SetVolume("endingBGM", bgmVolume_);
        }

        if (fadeAlpha_ >= 1.0f) {
            fadeAlpha_ = 1.0f;
            audio->Stop("endingBGM");
            sceneManager_->ChangeScene("Credit");
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

    // VHS effect
    bool asaVisible = (phase_ == Phase::AsaFadeIn || phase_ == Phase::Scrolling || phase_ == Phase::FadeOut);
    if (asaVisible) {
        vhsEffect_->SetVHSParams(
            timer_,
            0.20f, 0.12f, 0.15f, 0.4f, 0.25f, 0.4f, 0.10f
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

    bool asaVisible = (phase_ == Phase::AsaFadeIn || phase_ == Phase::Scrolling || phase_ == Phase::FadeOut);
    if (asaVisible) {
        asaSprite_->Draw();
    }

    if (phase_ == Phase::Scrolling || phase_ == Phase::FadeOut) {
        textSprite_->Draw();
    }

    if (fadeAlpha_ > 0.0f) {
        spriteCommon_->CommonDraw();
        fadeSprite_->Draw();
    }

    vhsEffect_->PostDraw();

    // 設定メニュー描画（最前面）
    if (settingsMenu_ && settingsMenu_->IsOpen()) {
        settingsMenu_->Draw();
    }
}

void EndingScene::Finalize() {
    settingsMenu_.reset();
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
