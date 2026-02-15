#include "IntroScene.h"
#include "SceneManager.h"
#include "Audio/AudioManager.h"

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

    // フェードスプライト（黒オーバーレイ）
    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(spriteCommon_, "Resources/textures/white1x1.png");
    fadeSprite_->SetPosition({0.0f, 0.0f});
    fadeSprite_->SetSize({1280.0f, 720.0f});
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, 1.0f});

    scrollY_ = kStartY;
    fadeAlpha_ = 1.0f;
    bgmVolume_ = 0.0f;
    phase_ = Phase::FadeIn;
    phaseTimer_ = 0.0f;

    // op.mp3の読み込みと再生（フェードイン中に開始）
    auto* audio = AudioManager::GetInstance();
    audio->LoadMP3("introBGM", "Resources/Audio/song/op.mp3");
    audio->SetVolume("introBGM", 0.0f);
    audio->Play("introBGM", false);

    // 設定メニューの初期化
    settingsMenu_ = std::make_unique<SettingsMenu>();
    settingsMenu_->Initialize(spriteCommon_, input_);
    settingsMenu_->AddBGMKey("introBGM");
}

void IntroScene::Update() {
    camera_->Update();

    // PostProcess resize on window size change
    {
        static uint32_t prevW = 0, prevH = 0;
        uint32_t curW = dxCommon_->GetCurrentWindowWidth();
        uint32_t curH = dxCommon_->GetCurrentWindowHeight();
        if (prevW != curW || prevH != curH) {
            if (prevW != 0 && vignetteEffect_) {
                vignetteEffect_->ResizeRenderTarget();
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

    switch (phase_) {
    case Phase::FadeIn:
        // 画面フェードイン
        fadeAlpha_ = 1.0f - phaseTimer_ / kFadeInDuration;
        if (fadeAlpha_ <= 0.0f) {
            fadeAlpha_ = 0.0f;
        }

        // BGMフェードイン
        bgmVolume_ = kBgmMaxVolume * (phaseTimer_ / kBgmFadeInDuration);
        if (bgmVolume_ > kBgmMaxVolume) bgmVolume_ = kBgmMaxVolume;
        audio->SetVolume("introBGM", bgmVolume_);

        if (phaseTimer_ >= kFadeInDuration) {
            phase_ = Phase::Scrolling;
            phaseTimer_ = 0.0f;
        }

        // スキップ
        if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
            phase_ = Phase::FadeOut;
            phaseTimer_ = 0.0f;
        }
        break;

    case Phase::Scrolling:
        scrollY_ -= kScrollSpeed * kDeltaTime;

        // テキストが流れ終わったらフェードアウトへ
        if (scrollY_ <= kEndY) {
            phase_ = Phase::FadeOut;
            phaseTimer_ = 0.0f;
        }

        // スキップ
        if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
            phase_ = Phase::FadeOut;
            phaseTimer_ = 0.0f;
        }
        break;

    case Phase::FadeOut:
        // 画面フェードアウト
        fadeAlpha_ = phaseTimer_ / kFadeOutDuration;
        if (fadeAlpha_ > 1.0f) fadeAlpha_ = 1.0f;

        // BGMフェードアウト
        bgmVolume_ = kBgmMaxVolume * (1.0f - phaseTimer_ / kBgmFadeOutDuration);
        if (bgmVolume_ < 0.0f) bgmVolume_ = 0.0f;
        audio->SetVolume("introBGM", bgmVolume_);

        // スクロールは継続
        scrollY_ -= kScrollSpeed * kDeltaTime;

        if (fadeAlpha_ >= 1.0f) {
            audio->Stop("introBGM");
            sceneManager_->ChangeScene("GamePlay");
            return;
        }
        break;
    }

    // スプライト更新
    textSprite_->SetPosition({640.0f, scrollY_});
    bgSprite_->Update();
    textSprite_->Update();
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, fadeAlpha_});
    fadeSprite_->Update();

    // VHSエフェクト
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

    // フェードオーバーレイ
    if (fadeAlpha_ > 0.0f) {
        spriteCommon_->CommonDraw();
        fadeSprite_->Draw();
    }

    vignetteEffect_->PostDraw();

    // 設定メニュー描画（最前面）
    if (settingsMenu_ && settingsMenu_->IsOpen()) {
        settingsMenu_->Draw();
    }
}

void IntroScene::Finalize() {
    settingsMenu_.reset();
    auto* audio = AudioManager::GetInstance();
    audio->Stop("introBGM");

    bgSprite_.reset();
    textSprite_.reset();
    fadeSprite_.reset();
    if (vignetteEffect_) {
        vignetteEffect_->Finalize();
        vignetteEffect_.reset();
    }
}
