#include "TitleScene.h"
#include "../../Engine/Resource/ResourcePreloader.h"
#include "SceneManager.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <random>
#ifdef _DEBUG
#include "imgui.h"
#endif

void TitleScene::Initialize() {
    camera_->SetTranslate({0.0f, 0.0f, -10.0f});

    // ノイズエフェクト（1パス目）
    noiseEffect_ = std::make_unique<PostProcess>();
    noiseEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::TitleNoise);

    // ビネットエフェクト（2パス目）
    vignetteEffect_ = std::make_unique<PostProcess>();
    vignetteEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::Horror);

    // VHSエフェクト（3パス目）
    vhsEffect_ = std::make_unique<PostProcess>();
    vhsEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::VHS);
    vhsEffect_->SetVHSParams(0.0f, 0.12f, 0.0f, 0.2f, 0.8f, 0.3f, 0.8f, 0.2f);

    // CRTブラウン管エフェクト（4パス目: 4:3アスペクト比）
    crtEffect_ = std::make_unique<PostProcess>();
    crtEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::CRT);

    // タイトルスプライトの初期化（通常の色で）
    titleBgSprite_ = std::make_unique<Sprite>();
    titleBgSprite_->Initialize(spriteCommon_, "Resources/textures/Title/Title_bg.png");

    titleBg2Sprite_ = std::make_unique<Sprite>();
    titleBg2Sprite_->Initialize(spriteCommon_, "Resources/textures/Title/Title_bg2.png");

    titleBgSprite_->setColor({0.4f, 0.4f, 0.4f, 1.0f});
    titleBg2Sprite_->setColor({0.35f, 0.35f, 0.35f, 1.0f});

    titleTextSprite_ = std::make_unique<Sprite>();
    titleTextSprite_->Initialize(spriteCommon_, "Resources/textures/title/title_text.png");

    hazimeruSprite_ = std::make_unique<Sprite>();
    hazimeruSprite_->Initialize(spriteCommon_, "Resources/textures/title/btn_start.png");
    hazimeruOriginalSize_ = hazimeruSprite_->GetSize();
    // Title_bg2の上部黒枠に配置
    hazimeruSprite_->SetPosition({ 640.0f, 500.0f });
    hazimeruSprite_->SetAnchorPoint({ 0.5f, 0.5f }); // 中心を基準に

    owaruSprite_ = std::make_unique<Sprite>();
    owaruSprite_->Initialize(spriteCommon_, "Resources/textures/title/btn_quit.png");
    owaruOriginalSize_ = owaruSprite_->GetSize();
    // Title_bg2の下部黒枠に配置
    owaruSprite_->SetPosition({ 640.0f, 590.0f });
    owaruSprite_->SetAnchorPoint({ 0.5f, 0.5f }); // 中心を基準に

    // 砂嵐スプライトの初期化（画面全体を覆う）
    noiseSprite_ = std::make_unique<Sprite>();
    noiseSprite_->Initialize(spriteCommon_, "Resources/textures/title/noise.png");
    noiseSprite_->SetPosition({ 0.0f, 0.0f });
    noiseSprite_->SetSize({ 1280.0f, 720.0f });
    noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f }); // 初期は透明

    ResourcePreloader::GetInstance()->PreloadAnimatedModelLightweight("human_walk", "Resources/models/player", "walk.gltf", dxCommon_);
    ResourcePreloader::GetInstance()->PreloadAnimatedModelLightweight("human_sneak", "Resources/models/player", "sneak_walk.gltf", dxCommon_);

    // === 色収差スプライト（タイトルテキスト用） ===
    titleTextRedSprite_ = std::make_unique<Sprite>();
    titleTextRedSprite_->Initialize(spriteCommon_, "Resources/textures/title/title_text.png");
    titleTextBlueSprite_ = std::make_unique<Sprite>();
    titleTextBlueSprite_->Initialize(spriteCommon_, "Resources/textures/title/title_text.png");

    // === 色収差スプライト（はじめる用） ===
    hazimeruRedSprite_ = std::make_unique<Sprite>();
    hazimeruRedSprite_->Initialize(spriteCommon_, "Resources/textures/title/btn_start.png");
    hazimeruRedSprite_->SetPosition({640.0f, 500.0f});
    hazimeruRedSprite_->SetAnchorPoint({0.5f, 0.5f});
    hazimeruBlueSprite_ = std::make_unique<Sprite>();
    hazimeruBlueSprite_->Initialize(spriteCommon_, "Resources/textures/title/btn_start.png");
    hazimeruBlueSprite_->SetPosition({640.0f, 500.0f});
    hazimeruBlueSprite_->SetAnchorPoint({0.5f, 0.5f});

    // === 色収差スプライト（おわる用） ===
    owaruRedSprite_ = std::make_unique<Sprite>();
    owaruRedSprite_->Initialize(spriteCommon_, "Resources/textures/title/btn_quit.png");
    owaruRedSprite_->SetPosition({640.0f, 590.0f});
    owaruRedSprite_->SetAnchorPoint({0.5f, 0.5f});
    owaruBlueSprite_ = std::make_unique<Sprite>();
    owaruBlueSprite_->Initialize(spriteCommon_, "Resources/textures/title/btn_quit.png");
    owaruBlueSprite_->SetPosition({640.0f, 590.0f});
    owaruBlueSprite_->SetAnchorPoint({0.5f, 0.5f});

    // 色収差アニメーション初期化
    chromaticTimer_ = 0.0f;
    glitchCooldown_ = 2.0f;
    glitchDuration_ = 0.0f;
    glitchOffsetX_ = 0.0f;
    glitchOffsetY_ = 0.0f;

    // ランダム砂嵐の初期タイミングを設定
    srand(static_cast<unsigned int>(time(nullptr)));
    nextNoiseTime_ = kMinNoiseInterval + static_cast<float>(rand()) / RAND_MAX * (kMaxNoiseInterval - kMinNoiseInterval);

    // フェードスプライト（黒オーバーレイ）
    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(spriteCommon_, "Resources/textures/common/white1x1.png");
    fadeSprite_->SetPosition({0.0f, 0.0f});
    fadeSprite_->SetSize({1280.0f, 720.0f});
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});

    // タイトルBGMの読み込み（再生は画面表示後にUpdate()で開始）
    AudioManager::GetInstance()->LoadMP3("titleBGM", "Resources/audio/bgm/title.mp3");
    AudioManager::GetInstance()->SetVolume("titleBGM", 0.075f);

    // 設定メニューの初期化
    settingsMenu_ = std::make_unique<SettingsMenu>();
    settingsMenu_->Initialize(spriteCommon_, input_);
}

void TitleScene::Update() {
    // 画面が表示されてから初回のUpdateでBGM再生開始
    if (!bgmStarted_) {
        AudioManager::GetInstance()->Play("titleBGM", true);
        bgmStarted_ = true;
    }

    camera_->Update();

    // PostProcess resize on window size change
    {
        static uint32_t prevW = 0, prevH = 0;
        uint32_t curW = dxCommon_->GetCurrentWindowWidth();
        uint32_t curH = dxCommon_->GetCurrentWindowHeight();
        if (prevW != curW || prevH != curH) {
            if (prevW != 0) {
                if (noiseEffect_) noiseEffect_->ResizeRenderTarget();
                if (vignetteEffect_) vignetteEffect_->ResizeRenderTarget();
                if (vhsEffect_) vhsEffect_->ResizeRenderTarget();
                if (crtEffect_) crtEffect_->ResizeRenderTarget();
            }
            prevW = curW;
            prevH = curH;
        }
    }

    float deltaTime = 1.0f / 60.0f;

    // ESCでゲーム終了
    if (input_->TriggerKey(DIK_ESCAPE)) {
        sceneManager_->RequestExit();
        return;
    }

    // 初回砂嵐エフェクト
    if (showInitialNoise_) {
        initialNoiseTimer_ += deltaTime;

        // 画像をそのまま表示
        noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        if (initialNoiseTimer_ >= kInitialNoiseDuration) {
            showInitialNoise_ = false;
            noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        }
    }
    // ランダム砂嵐エフェクト
    else {
        randomNoiseTimer_ += deltaTime;

        // 次の砂嵐発生タイミングに到達
        if (!showRandomNoise_ && randomNoiseTimer_ >= nextNoiseTime_) {
            showRandomNoise_ = true;
            randomNoiseTimer_ = 0.0f;
        }

        // 砂嵐表示中
        if (showRandomNoise_) {
            // 画像をそのまま表示
            noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });

            if (randomNoiseTimer_ >= kRandomNoiseDuration) {
                showRandomNoise_ = false;
                noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f });
                randomNoiseTimer_ = 0.0f;
                // 次の砂嵐タイミングをランダムに設定
                nextNoiseTime_ = kMinNoiseInterval + static_cast<float>(rand()) / RAND_MAX * (kMaxNoiseInterval - kMinNoiseInterval);
            }
        }
    }

    // ノイズタイマー更新
    noiseTimer_ += deltaTime;

    // キーボード入力フラグ
    bool keyPressed = false;

    // W/上矢印キーでメニュー選択を上に
    if (input_->TriggerKey(DIK_W) || input_->TriggerKey(DIK_UP)) {
        currentSelection_ = MenuSelection::Start;
        keyPressed = true;
    }
    // S/下矢印キーでメニュー選択を下に
    if (input_->TriggerKey(DIK_S) || input_->TriggerKey(DIK_DOWN)) {
        currentSelection_ = MenuSelection::Exit;
        keyPressed = true;
    }

    // マウスでホバー検知（キーが押されていない時のみ）
    bool hazimeruHovered = false;
    bool owaruHovered = false;
    if (!keyPressed) {
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        HWND hwnd = FindWindowW(L"CG2WindowClass", nullptr);
        ScreenToClient(hwnd, &cursorPos);
        // クライアント座標を論理座標(1280x720)にスケーリング
        RECT rc;
        GetClientRect(hwnd, &rc);
        float clientW = static_cast<float>(rc.right - rc.left);
        float clientH = static_cast<float>(rc.bottom - rc.top);
        Vector2 mousePos = {
            static_cast<float>(cursorPos.x) * (static_cast<float>(WinApp::kClientWidth) / clientW),
            static_cast<float>(cursorPos.y) * (static_cast<float>(WinApp::kClientHeight) / clientH)
        };

        // アンカーポイントを考慮した判定範囲を計算
        Vector2 hazimeruPos = hazimeruSprite_->GetPosition();
        Vector2 hazimeruMin = { hazimeruPos.x - hazimeruOriginalSize_.x * 0.5f, hazimeruPos.y - hazimeruOriginalSize_.y * 0.5f };
        Vector2 hazimeruMax = { hazimeruPos.x + hazimeruOriginalSize_.x * 0.5f, hazimeruPos.y + hazimeruOriginalSize_.y * 0.5f };

        Vector2 owaruPos = owaruSprite_->GetPosition();
        Vector2 owaruMin = { owaruPos.x - owaruOriginalSize_.x * 0.5f, owaruPos.y - owaruOriginalSize_.y * 0.5f };
        Vector2 owaruMax = { owaruPos.x + owaruOriginalSize_.x * 0.5f, owaruPos.y + owaruOriginalSize_.y * 0.5f };

        if (mousePos.x >= hazimeruMin.x && mousePos.x <= hazimeruMax.x &&
            mousePos.y >= hazimeruMin.y && mousePos.y <= hazimeruMax.y) {
            hazimeruHovered = true;
            currentSelection_ = MenuSelection::Start;
        }
        if (mousePos.x >= owaruMin.x && mousePos.x <= owaruMax.x &&
            mousePos.y >= owaruMin.y && mousePos.y <= owaruMax.y) {
            owaruHovered = true;
            currentSelection_ = MenuSelection::Exit;
        }
    }

    // === メニューグリッチ演出 ===
    menuGlitchTimer_ += deltaTime;

    // Layer 1: 微細ジッター (常時±1.5px揺れ)
    float jitterX = std::sin(menuGlitchTimer_ * 15.0f) * 1.5f;
    float jitterY = std::cos(menuGlitchTimer_ * 11.0f) * 1.5f;

    // Layer 2: アルファフリッカー (輝度変動 0.7~1.0 + まれに急降下)
    float flicker = std::sin(menuGlitchTimer_ * 8.0f) * std::sin(menuGlitchTimer_ * 13.0f);
    flicker = flicker * flicker; // 0.0~1.0
    float brightness = 0.7f + flicker * 0.3f;
    // まれに急降下 (sin積が低いタイミング)
    float dip = std::sin(menuGlitchTimer_ * 3.7f);
    if (dip > 0.92f) {
        brightness = 0.4f;
    }

    // Layer 3: グリッチバースト (0.8~3秒間隔で大きなXオフセット)
    if (menuGlitchBurstDuration_ > 0.0f) {
        menuGlitchBurstDuration_ -= deltaTime;
    } else {
        menuGlitchBurstOffsetX_ = 0.0f;
        menuGlitchBurstOffsetY_ = 0.0f;
        menuGlitchBurstCooldown_ -= deltaTime;
        if (menuGlitchBurstCooldown_ <= 0.0f) {
            static std::mt19937 menuRng(std::random_device{}());
            std::uniform_real_distribution<float> burstDurDist(0.03f, 0.12f);
            std::uniform_real_distribution<float> burstCoolDist(0.8f, 3.0f);
            std::uniform_real_distribution<float> burstOffDist(15.0f, 35.0f);
            std::uniform_real_distribution<float> burstYDist(-3.0f, 3.0f);
            menuGlitchBurstDuration_ = burstDurDist(menuRng);
            menuGlitchBurstCooldown_ = burstCoolDist(menuRng);
            menuGlitchBurstOffsetX_ = burstOffDist(menuRng);
            menuGlitchBurstOffsetY_ = burstYDist(menuRng);
        }
    }

    bool isBursting = (menuGlitchBurstDuration_ > 0.0f);

    // 選択中ボタンにグリッチ適用、非選択は暗い固定色
    float selPosOffX = jitterX + menuGlitchBurstOffsetX_;
    float selPosOffY = jitterY + menuGlitchBurstOffsetY_;

    if (currentSelection_ == MenuSelection::Start || hazimeruHovered) {
        hazimeruSprite_->SetPosition({640.0f + selPosOffX, 500.0f + selPosOffY});
        hazimeruSprite_->setColor({brightness, brightness, brightness, 1.0f});

        owaruSprite_->SetPosition({640.0f, 590.0f});
        owaruSprite_->setColor({0.5f, 0.5f, 0.5f, 1.0f});
    } else {
        owaruSprite_->SetPosition({640.0f + selPosOffX, 590.0f + selPosOffY});
        owaruSprite_->setColor({brightness, brightness, brightness, 1.0f});

        hazimeruSprite_->SetPosition({640.0f, 500.0f});
        hazimeruSprite_->setColor({0.5f, 0.5f, 0.5f, 1.0f});
    }

    // スプライトの更新
    titleBgSprite_->Update();
    titleBg2Sprite_->Update();
    titleTextSprite_->Update();
    hazimeruSprite_->Update();
    owaruSprite_->Update();
    noiseSprite_->Update();

    // === 色収差アニメーション ===
    chromaticTimer_ += deltaTime;

    float baseOffset = std::sin(chromaticTimer_ * 2.5f) * 3.0f;

    if (glitchDuration_ > 0.0f) {
        glitchDuration_ -= deltaTime;
    } else {
        glitchOffsetX_ = 0.0f;
        glitchOffsetY_ = 0.0f;
        glitchCooldown_ -= deltaTime;
        if (glitchCooldown_ <= 0.0f) {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> durDist(0.05f, 0.15f);
            std::uniform_real_distribution<float> coolDist(1.5f, 5.0f);
            std::uniform_real_distribution<float> offsetDist(8.0f, 20.0f);
            std::uniform_real_distribution<float> yOffsetDist(-5.0f, 5.0f);
            glitchDuration_ = durDist(rng);
            glitchCooldown_ = coolDist(rng);
            glitchOffsetX_ = offsetDist(rng);
            glitchOffsetY_ = yOffsetDist(rng);
        }
    }

    float totalOffsetX = baseOffset + glitchOffsetX_;
    float totalOffsetY = glitchOffsetY_;

    // タイトルテキスト色収差（常時）
    Vector2 titleTextPos = titleTextSprite_->GetPosition();
    titleTextRedSprite_->SetPosition({titleTextPos.x - totalOffsetX, titleTextPos.y + totalOffsetY});
    titleTextRedSprite_->setColor({1.0f, 0.2f, 0.0f, 0.7f});
    titleTextRedSprite_->Update();
    titleTextBlueSprite_->SetPosition({titleTextPos.x + totalOffsetX, titleTextPos.y - totalOffsetY});
    titleTextBlueSprite_->setColor({0.0f, 0.4f, 0.8f, 0.35f});
    titleTextBlueSprite_->Update();
    titleTextSprite_->setColor({1.0f, 0.08f, 0.05f, 1.0f});

    // メニュー色収差（選択中の項目のみ、バースト中は増幅）
    float menuChromaticMul = isBursting ? 2.5f : 1.0f;
    float menuOffX = totalOffsetX * menuChromaticMul;
    float menuOffY = totalOffsetY * menuChromaticMul;

    if (currentSelection_ == MenuSelection::Start) {
        float bx = 640.0f + selPosOffX;
        float by = 500.0f + selPosOffY;
        hazimeruRedSprite_->SetPosition({bx - menuOffX, by + menuOffY});
        hazimeruRedSprite_->setColor({1.0f, 0.0f, 0.0f, 0.6f});
        hazimeruRedSprite_->Update();
        hazimeruBlueSprite_->SetPosition({bx + menuOffX, by - menuOffY});
        hazimeruBlueSprite_->setColor({0.0f, 0.0f, 1.0f, 0.6f});
        hazimeruBlueSprite_->Update();
        owaruRedSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        owaruRedSprite_->Update();
        owaruBlueSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        owaruBlueSprite_->Update();
    } else {
        float bx = 640.0f + selPosOffX;
        float by = 590.0f + selPosOffY;
        owaruRedSprite_->SetPosition({bx - menuOffX, by + menuOffY});
        owaruRedSprite_->setColor({1.0f, 0.0f, 0.0f, 0.6f});
        owaruRedSprite_->Update();
        owaruBlueSprite_->SetPosition({bx + menuOffX, by - menuOffY});
        owaruBlueSprite_->setColor({0.0f, 0.0f, 1.0f, 0.6f});
        owaruBlueSprite_->Update();
        hazimeruRedSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        hazimeruRedSprite_->Update();
        hazimeruBlueSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        hazimeruBlueSprite_->Update();
    }

    // ノイズエフェクトのパラメータ更新
    time_ += 1.0f / 60.0f;
    noiseEffect_->SetTitleNoiseParams(
        time_,
        0.04f,   // grainIntensity
        0.10f,   // scanlineIntensity
        350.0f,  // scanlineCount
        0.6f,    // glitchIntensity
        0.12f,   // glitchFrequency
        0.015f,  // chromaticStrength
        0.8f     // vignetteIntensity
    );

    // ビネットエフェクトのパラメータ更新
    vignetteEffect_->SetHorrorParams(time_, 0.0f, 0.0f, 0.0f, 0.8f);

    // VHSエフェクトのパラメータ更新
    if (vhsEffect_) {
        vhsEffect_->SetVHSParams(time_, 0.12f, 0.0f, 0.2f, 0.8f, 0.3f, 0.8f, 0.2f);
    }

    // CRTブラウン管エフェクトのパラメータ更新
    if (crtEffect_) {
        float aspect = static_cast<float>(dxCommon_->GetCurrentWindowWidth())
                     / static_cast<float>(dxCommon_->GetCurrentWindowHeight());
        crtEffect_->SetCRTParams(0.06f, 0.08f, 0.30f, 0.008f, aspect, 4.0f / 3.0f);
    }

    // フェードアウト中は入力を無視
    if (fadingOut_) {
        constexpr float kDeltaTime = 1.0f / 60.0f;
        fadeAlpha_ += kDeltaTime / kFadeOutDuration;

        // BGMフェードアウト
        float bgmVol = 0.075f * (1.0f - fadeAlpha_);
        if (bgmVol < 0.0f) bgmVol = 0.0f;
        AudioManager::GetInstance()->SetVolume("titleBGM", bgmVol);

        if (fadeAlpha_ >= 1.0f) {
            fadeAlpha_ = 1.0f;
            AudioManager::GetInstance()->Stop("titleBGM");
            sceneManager_->ChangeScene("Intro");
            return;
        }
        fadeSprite_->setColor({0.0f, 0.0f, 0.0f, fadeAlpha_});
        fadeSprite_->Update();
        return;
    }

    // マウスクリックで決定
    DIMOUSESTATE mouseState;
    if (SUCCEEDED(input_->GetMouseState(&mouseState))) {
        if (mouseState.rgbButtons[0] & 0x80) {
            if (hazimeruHovered) {
                fadingOut_ = true;
            }
            if (owaruHovered) {
                sceneManager_->RequestExit();
            }
        }
    }

    // SPACEまたはENTERで決定
    if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
        if (currentSelection_ == MenuSelection::Start) {
            fadingOut_ = true;
        } else {
            sceneManager_->RequestExit();
        }
    }

}

void TitleScene::Draw() {
    // 1パス目: ノイズエフェクトのRTにシーン描画
    noiseEffect_->PreDraw();

    spriteCommon_->CommonDraw();

    // 背景
    titleBgSprite_->Draw();
    titleBg2Sprite_->Draw();

    if (showInitialNoise_ || showRandomNoise_) {
        noiseSprite_->Draw();
    }

    // タイトルテキスト（色収差: 赤→青→本体）
    if (titleTextRedSprite_) titleTextRedSprite_->Draw();
    if (titleTextBlueSprite_) titleTextBlueSprite_->Draw();
    titleTextSprite_->Draw();

    // メニュー（色収差: 赤→青→本体）
    if (hazimeruRedSprite_) hazimeruRedSprite_->Draw();
    if (hazimeruBlueSprite_) hazimeruBlueSprite_->Draw();
    hazimeruSprite_->Draw();

    if (owaruRedSprite_) owaruRedSprite_->Draw();
    if (owaruBlueSprite_) owaruBlueSprite_->Draw();
    owaruSprite_->Draw();

    // チェーン: TitleNoise → Horror(ビネット) → VHS → CRT → Backbuffer
    if (vhsEffect_ && crtEffect_) {
        noiseEffect_->PostDrawTo(vignetteEffect_.get());
        vignetteEffect_->PostDrawTo(vhsEffect_.get());
        vhsEffect_->PostDrawTo(crtEffect_.get());
        crtEffect_->PostDraw();
    } else if (vhsEffect_) {
        noiseEffect_->PostDrawTo(vignetteEffect_.get());
        vignetteEffect_->PostDrawTo(vhsEffect_.get());
        vhsEffect_->PostDraw();
    } else {
        noiseEffect_->PostDrawTo(vignetteEffect_.get());
        vignetteEffect_->PostDraw();
    }

    // フェードアウト描画（ポストプロセス外）
    if (fadingOut_ && fadeAlpha_ > 0.0f) {
        spriteCommon_->CommonDraw();
        fadeSprite_->Draw();
    }

    // 設定メニュー描画（最前面）
    if (settingsMenu_ && settingsMenu_->IsOpen()) {
        settingsMenu_->Draw();
    }
}

bool TitleScene::CheckMouseHover(const Vector2& mousePos, const Vector2& spritePos, const Vector2& spriteSize) {
    return mousePos.x >= spritePos.x && mousePos.x <= spritePos.x + spriteSize.x &&
           mousePos.y >= spritePos.y && mousePos.y <= spritePos.y + spriteSize.y;
}

void TitleScene::Finalize() {
    OutputDebugStringA("TitleScene::Finalize() called\n");

    // 設定メニューの解放
    settingsMenu_.reset();

    // タイトルBGMの停止
    AudioManager::GetInstance()->Stop("titleBGM");

    // スプライトの解放
    if (titleBgSprite_) {
        OutputDebugStringA("  Releasing titleBgSprite_\n");
        titleBgSprite_.reset();
    }
    if (titleBg2Sprite_) {
        titleBg2Sprite_.reset();
    }
    if (titleTextSprite_) {
        OutputDebugStringA("  Releasing titleTextSprite_\n");
        titleTextSprite_.reset();
    }
    if (hazimeruSprite_) {
        hazimeruSprite_.reset();
    }
    if (owaruSprite_) {
        owaruSprite_.reset();
    }
    if (noiseSprite_) {
        noiseSprite_.reset();
    }
    if (fadeSprite_) {
        fadeSprite_.reset();
    }

    // 色収差スプライトの解放
    titleTextRedSprite_.reset();
    titleTextBlueSprite_.reset();
    hazimeruRedSprite_.reset();
    hazimeruBlueSprite_.reset();
    owaruRedSprite_.reset();
    owaruBlueSprite_.reset();

    if (noiseEffect_) {
        OutputDebugStringA("  Finalizing noiseEffect_\n");
        noiseEffect_->Finalize();
        noiseEffect_.reset();
    }
    if (vignetteEffect_) {
        OutputDebugStringA("  Finalizing vignetteEffect_\n");
        vignetteEffect_->Finalize();
        vignetteEffect_.reset();
    }
    if (vhsEffect_) {
        OutputDebugStringA("  Finalizing vhsEffect_\n");
        vhsEffect_->Finalize();
        vhsEffect_.reset();
    }
    if (crtEffect_) {
        OutputDebugStringA("  Finalizing crtEffect_\n");
        crtEffect_->Finalize();
        crtEffect_.reset();
    }

    OutputDebugStringA("TitleScene::Finalize() completed\n");
}