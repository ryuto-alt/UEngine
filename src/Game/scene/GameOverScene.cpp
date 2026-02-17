#include "GameOverScene.h"
#include "../../Engine/Resource/ResourcePreloader.h"
#include "SceneManager.h"
#include <random>
#ifdef _DEBUG
#include "imgui.h"
#endif

void GameOverScene::Initialize() {
    if (!dxCommon_ || !srvManager_ || !camera_) {
        OutputDebugStringA("GameOverScene::Initialize - Critical error: Required pointers are null!\n");
        return;
    }

    camera_->SetTranslate({0.0f, 0.0f, -10.0f});

    // ホラーエフェクトの初期化
    horrorEffect_ = std::make_unique<PostProcess>();
    horrorEffect_->Initialize(dxCommon_, srvManager_);

    // VHSエフェクトの初期化
    vhsEffect_ = std::make_unique<PostProcess>();
    vhsEffect_->Initialize(dxCommon_, srvManager_, PostProcess::EffectType::VHS);
    vhsEffect_->SetVHSParams(
        0.0f,   // time
        0.15f,  // scanlineIntensity
        0.08f,  // noiseIntensity
        0.3f,   // trackingError
        1.2f,   // chromaticAberration
        0.4f,   // colorBleed
        0.7f,   // sharpness
        0.3f    // tapeCrease
    );

    // TVノイズ - フェードインに合わせて強めに開始
    if (horrorEffect_) {
        horrorEffect_->SetHorrorParams(
            0.0f,   // time
            0.8f,   // noiseIntensity
            0.2f,   // distortionIntensity
            0.15f,  // bloodAmount（微量の血エフェクト）
            0.6f    // vignetteIntensity（強めのビネット）
        );
    }

    // === 背景レイヤー ===
    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/gameover_bg.png");
    backgroundSprite_->SetPosition({640.0f, 360.0f});
    backgroundSprite_->SetAnchorPoint({0.5f, 0.5f});

    // 血のオーバーレイ（上部から垂れる血）
    bloodOverlaySprite_ = std::make_unique<Sprite>();
    bloodOverlaySprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/blood_overlay.png");
    bloodOverlaySprite_->SetPosition({640.0f, 0.0f});
    bloodOverlaySprite_->SetAnchorPoint({0.5f, 0.0f});

    // === テキストレイヤー ===
    gameOverTextSprite_ = std::make_unique<Sprite>();
    gameOverTextSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/gameover_text.png");
    gameOverTextSprite_->SetPosition({640.0f, 220.0f});
    gameOverTextSprite_->SetAnchorPoint({0.5f, 0.5f});

    // === メニューボタン ===
    continueSprite_ = std::make_unique<Sprite>();
    continueSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/continue_btn.png");
    continueOriginalSize_ = continueSprite_->GetSize();
    continueSprite_->SetPosition({640.0f, 460.0f});
    continueSprite_->SetAnchorPoint({0.5f, 0.5f});

    quitSprite_ = std::make_unique<Sprite>();
    quitSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/quit_btn.png");
    quitOriginalSize_ = quitSprite_->GetSize();
    quitSprite_->SetPosition({640.0f, 540.0f});
    quitSprite_->SetAnchorPoint({0.5f, 0.5f});

    // セレクター（選択矢印）
    selectorSprite_ = std::make_unique<Sprite>();
    selectorSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/selector.png");
    selectorSprite_->SetAnchorPoint({0.5f, 0.5f});

    // === 色収差スプライト（GAME OVER用） ===
    gameOverRedSprite_ = std::make_unique<Sprite>();
    gameOverRedSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/gameover_text.png");
    gameOverRedSprite_->SetPosition({640.0f, 220.0f});
    gameOverRedSprite_->SetAnchorPoint({0.5f, 0.5f});

    gameOverBlueSprite_ = std::make_unique<Sprite>();
    gameOverBlueSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/gameover_text.png");
    gameOverBlueSprite_->SetPosition({640.0f, 220.0f});
    gameOverBlueSprite_->SetAnchorPoint({0.5f, 0.5f});

    // === 色収差スプライト（Continue用） ===
    continueRedSprite_ = std::make_unique<Sprite>();
    continueRedSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/continue_btn.png");
    continueRedSprite_->SetPosition({640.0f, 460.0f});
    continueRedSprite_->SetAnchorPoint({0.5f, 0.5f});

    continueBlueSprite_ = std::make_unique<Sprite>();
    continueBlueSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/continue_btn.png");
    continueBlueSprite_->SetPosition({640.0f, 460.0f});
    continueBlueSprite_->SetAnchorPoint({0.5f, 0.5f});

    // === 色収差スプライト（Quit用） ===
    quitRedSprite_ = std::make_unique<Sprite>();
    quitRedSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/quit_btn.png");
    quitRedSprite_->SetPosition({640.0f, 540.0f});
    quitRedSprite_->SetAnchorPoint({0.5f, 0.5f});

    quitBlueSprite_ = std::make_unique<Sprite>();
    quitBlueSprite_->Initialize(spriteCommon_, "Resources/textures/GameOver/quit_btn.png");
    quitBlueSprite_->SetPosition({640.0f, 540.0f});
    quitBlueSprite_->SetAnchorPoint({0.5f, 0.5f});

    // 色収差アニメーション初期化
    chromaticTimer_ = 0.0f;
    glitchCooldown_ = 2.0f;
    glitchDuration_ = 0.0f;
    glitchOffsetX_ = 0.0f;
    glitchOffsetY_ = 0.0f;

    // フェードイン開始
    fadeInTimer_ = 0.0f;

    // 設定メニューの初期化
    settingsMenu_ = std::make_unique<SettingsMenu>();
    settingsMenu_->Initialize(spriteCommon_, input_);
}

void GameOverScene::Update() {
    if (!camera_ || !input_) {
        return;
    }

    camera_->Update();

    // PostProcess resize on window size change
    {
        static uint32_t prevW = 0, prevH = 0;
        uint32_t curW = dxCommon_->GetCurrentWindowWidth();
        uint32_t curH = dxCommon_->GetCurrentWindowHeight();
        if (prevW != curW || prevH != curH) {
            if (prevW != 0) {
                if (horrorEffect_) horrorEffect_->ResizeRenderTarget();
                if (vhsEffect_) vhsEffect_->ResizeRenderTarget();
            }
            prevW = curW;
            prevH = curH;
        }
    }

    float deltaTime = 1.0f / 60.0f;

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
        settingsMenu_->Update(deltaTime);
        return;
    }

    time_ += deltaTime;
    pulseTimer_ += deltaTime;

    // フェードインタイマー更新
    if (fadeInTimer_ < fadeInDuration_) {
        fadeInTimer_ += deltaTime;
        if (fadeInTimer_ > fadeInDuration_) {
            fadeInTimer_ = fadeInDuration_;
        }
    }
    float fadeAlpha = fadeInTimer_ / fadeInDuration_; // 0.0 → 1.0

    // ホラーエフェクトのパラメータを更新
    if (horrorEffect_) {
        // フェードインに合わせてノイズを減少させる
        float noiseDecay = 1.0f - fadeAlpha * 0.5f; // 1.0 → 0.5
        horrorEffect_->SetHorrorParams(
            time_,
            0.8f * noiseDecay,   // noiseIntensity（徐々に落ち着く）
            0.2f * noiseDecay,   // distortionIntensity
            0.15f * fadeAlpha,   // bloodAmount（徐々に現れる）
            0.6f                 // vignetteIntensity
        );
    }

    // VHSエフェクトのパラメータを更新
    if (vhsEffect_) {
        vhsEffect_->SetVHSParams(
            time_,
            0.15f,  // scanlineIntensity
            0.08f,  // noiseIntensity
            0.3f,   // trackingError
            1.2f,   // chromaticAberration
            0.4f,   // colorBleed
            0.7f,   // sharpness
            0.3f    // tapeCrease
        );
    }

    // === 色収差アニメーション ===
    chromaticTimer_ += deltaTime;

    // ベースオフセット（sin波のゆるい揺れ）
    float baseOffset = std::sin(chromaticTimer_ * 2.5f) * 3.0f;

    // グリッチスパイク処理
    if (glitchDuration_ > 0.0f) {
        glitchDuration_ -= deltaTime;
    } else {
        glitchOffsetX_ = 0.0f;
        glitchOffsetY_ = 0.0f;
        glitchCooldown_ -= deltaTime;
        if (glitchCooldown_ <= 0.0f) {
            // グリッチ発生（50〜150ms）
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

    // === スプライトのフェードイン ===
    // 背景は常に表示
    backgroundSprite_->setColor({1.0f, 1.0f, 1.0f, 1.0f});
    backgroundSprite_->Update();

    // 血のオーバーレイ（徐々に現れる）
    float bloodAlpha = std::min(1.0f, fadeAlpha * 1.5f);
    bloodOverlaySprite_->setColor({1.0f, 1.0f, 1.0f, bloodAlpha});
    bloodOverlaySprite_->Update();

    // GAME OVERテキスト（フェードイン + 色収差）
    // 赤コピー（左にオフセット）
    gameOverRedSprite_->SetPosition({640.0f - totalOffsetX, 220.0f + totalOffsetY});
    gameOverRedSprite_->setColor({1.0f, 0.0f, 0.0f, fadeAlpha * 0.6f});
    gameOverRedSprite_->Update();

    // 青コピー（右にオフセット）
    gameOverBlueSprite_->SetPosition({640.0f + totalOffsetX, 220.0f - totalOffsetY});
    gameOverBlueSprite_->setColor({0.0f, 0.0f, 1.0f, fadeAlpha * 0.6f});
    gameOverBlueSprite_->Update();

    // 本体（緑寄り補正）
    gameOverTextSprite_->setColor({0.3f, 1.0f, 0.3f, fadeAlpha});
    gameOverTextSprite_->Update();

    // メニューボタン（遅延フェードイン）
    float menuAlpha = (std::max)(0.0f, (fadeAlpha - 0.4f) / 0.6f);

    // キーボード入力フラグ
    bool keyPressed = false;

    // メニューが表示されてから入力を受け付ける
    if (menuAlpha > 0.5f) {
        // W/上矢印キーでメニュー選択を上に
        if (input_->TriggerKey(DIK_W) || input_->TriggerKey(DIK_UP)) {
            currentSelection_ = MenuSelection::Continue;
            keyPressed = true;
        }
        // S/下矢印キーでメニュー選択を下に
        if (input_->TriggerKey(DIK_S) || input_->TriggerKey(DIK_DOWN)) {
            currentSelection_ = MenuSelection::Quit;
            keyPressed = true;
        }
    }

    // マウスでホバー検知（キーが押されていない時のみ）
    bool continueHovered = false;
    bool quitHovered = false;
    if (!keyPressed && menuAlpha > 0.5f) {
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
        Vector2 continuePos = continueSprite_->GetPosition();
        Vector2 continueMin = { continuePos.x - continueOriginalSize_.x * 0.5f, continuePos.y - continueOriginalSize_.y * 0.5f };
        Vector2 continueMax = { continuePos.x + continueOriginalSize_.x * 0.5f, continuePos.y + continueOriginalSize_.y * 0.5f };

        Vector2 quitPos = quitSprite_->GetPosition();
        Vector2 quitMin = { quitPos.x - quitOriginalSize_.x * 0.5f, quitPos.y - quitOriginalSize_.y * 0.5f };
        Vector2 quitMax = { quitPos.x + quitOriginalSize_.x * 0.5f, quitPos.y + quitOriginalSize_.y * 0.5f };

        if (mousePos.x >= continueMin.x && mousePos.x <= continueMax.x &&
            mousePos.y >= continueMin.y && mousePos.y <= continueMax.y) {
            continueHovered = true;
            currentSelection_ = MenuSelection::Continue;
        }
        if (mousePos.x >= quitMin.x && mousePos.x <= quitMax.x &&
            mousePos.y >= quitMin.y && mousePos.y <= quitMax.y) {
            quitHovered = true;
            currentSelection_ = MenuSelection::Quit;
        }
    }

    // 選択状態に応じた視覚フィードバック（パルスアニメーション付き）
    float pulse = 0.85f + 0.15f * std::sin(pulseTimer_ * 3.0f);
    if (currentSelection_ == MenuSelection::Continue) {
        continueSprite_->setColor({1.0f, 1.0f, 1.0f, menuAlpha * pulse});
        quitSprite_->setColor({0.4f, 0.4f, 0.4f, menuAlpha * 0.7f});
        // セレクター位置を「コンティニュー」の左に
        selectorSprite_->SetPosition({640.0f - continueOriginalSize_.x * 0.5f - 25.0f, 460.0f});
        selectorSprite_->setColor({1.0f, 1.0f, 1.0f, menuAlpha});
    } else {
        quitSprite_->setColor({1.0f, 1.0f, 1.0f, menuAlpha * pulse});
        continueSprite_->setColor({0.4f, 0.4f, 0.4f, menuAlpha * 0.7f});
        // セレクター位置を「タイトルへ戻る」の左に
        selectorSprite_->SetPosition({640.0f - quitOriginalSize_.x * 0.5f - 25.0f, 540.0f});
        selectorSprite_->setColor({1.0f, 1.0f, 1.0f, menuAlpha});
    }

    // === メニュー色収差スプライト更新 ===
    if (currentSelection_ == MenuSelection::Continue) {
        // Continue: 選択中 → 色収差あり
        continueRedSprite_->SetPosition({640.0f - totalOffsetX, 460.0f + totalOffsetY});
        continueRedSprite_->setColor({1.0f, 0.0f, 0.0f, menuAlpha * 0.6f});
        continueRedSprite_->Update();

        continueBlueSprite_->SetPosition({640.0f + totalOffsetX, 460.0f - totalOffsetY});
        continueBlueSprite_->setColor({0.0f, 0.0f, 1.0f, menuAlpha * 0.6f});
        continueBlueSprite_->Update();

        // Quit: 非選択 → 色収差なし
        quitRedSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        quitRedSprite_->Update();
        quitBlueSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        quitBlueSprite_->Update();
    } else {
        // Quit: 選択中 → 色収差あり
        quitRedSprite_->SetPosition({640.0f - totalOffsetX, 540.0f + totalOffsetY});
        quitRedSprite_->setColor({1.0f, 0.0f, 0.0f, menuAlpha * 0.6f});
        quitRedSprite_->Update();

        quitBlueSprite_->SetPosition({640.0f + totalOffsetX, 540.0f - totalOffsetY});
        quitBlueSprite_->setColor({0.0f, 0.0f, 1.0f, menuAlpha * 0.6f});
        quitBlueSprite_->Update();

        // Continue: 非選択 → 色収差なし
        continueRedSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        continueRedSprite_->Update();
        continueBlueSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});
        continueBlueSprite_->Update();
    }

    // スプライトの更新
    continueSprite_->Update();
    quitSprite_->Update();
    selectorSprite_->Update();

    // マウスクリックで決定（メニュー表示後のみ）
    if (menuAlpha > 0.5f) {
        DIMOUSESTATE mouseState;
        if (SUCCEEDED(input_->GetMouseState(&mouseState))) {
            if (mouseState.rgbButtons[0] & 0x80) { // 左クリック
                if (continueHovered) {
                    sceneManager_->ChangeScene("GamePlay");
                }
                if (quitHovered) {
                    sceneManager_->ChangeScene("Title");
                }
            }
        }

        // SPACEまたはENTERで決定
        if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
            if (currentSelection_ == MenuSelection::Continue) {
                sceneManager_->ChangeScene("GamePlay");
            } else {
                sceneManager_->ChangeScene("Title");
            }
        }
    }
}

void GameOverScene::Draw() {
    // TVノイズポストプロセス用のレンダーターゲットに描画開始
    if (horrorEffect_) {
        horrorEffect_->PreDraw();
    }

    if (spriteCommon_) {
        spriteCommon_->CommonDraw();
    }

    // === 描画順序（背景→前景） ===
    // 1. 背景
    if (backgroundSprite_) backgroundSprite_->Draw();

    // 2. 血のオーバーレイ
    if (bloodOverlaySprite_) bloodOverlaySprite_->Draw();

    // 3. GAME OVERテキスト（赤→青→本体の順で色収差描画）
    if (gameOverRedSprite_) gameOverRedSprite_->Draw();
    if (gameOverBlueSprite_) gameOverBlueSprite_->Draw();
    if (gameOverTextSprite_) gameOverTextSprite_->Draw();

    // 4. メニューボタン（赤→青→本体の順で色収差描画）
    if (continueRedSprite_) continueRedSprite_->Draw();
    if (continueBlueSprite_) continueBlueSprite_->Draw();
    if (continueSprite_) continueSprite_->Draw();

    if (quitRedSprite_) quitRedSprite_->Draw();
    if (quitBlueSprite_) quitBlueSprite_->Draw();
    if (quitSprite_) quitSprite_->Draw();

    // 5. セレクター矢印
    if (selectorSprite_) selectorSprite_->Draw();

    // ホラー→VHSチェーン描画
    if (horrorEffect_ && vhsEffect_) {
        horrorEffect_->PostDrawTo(vhsEffect_.get());
        vhsEffect_->PostDraw();
    } else if (horrorEffect_) {
        horrorEffect_->PostDraw();
    }

    // 設定メニュー描画（最前面）
    if (settingsMenu_ && settingsMenu_->IsOpen()) {
        settingsMenu_->Draw();
    }
}

void GameOverScene::Finalize() {
    settingsMenu_.reset();
    if (horrorEffect_) {
        horrorEffect_->Finalize();
        horrorEffect_.reset();
    }
    if (vhsEffect_) {
        vhsEffect_->Finalize();
        vhsEffect_.reset();
    }

    backgroundSprite_.reset();
    bloodOverlaySprite_.reset();
    gameOverTextSprite_.reset();
    gameOverRedSprite_.reset();
    gameOverBlueSprite_.reset();
    continueSprite_.reset();
    continueRedSprite_.reset();
    continueBlueSprite_.reset();
    quitSprite_.reset();
    quitRedSprite_.reset();
    quitBlueSprite_.reset();
    selectorSprite_.reset();
}

bool GameOverScene::CheckMouseHover(const Vector2& mousePos, const Vector2& spritePos, const Vector2& spriteSize) {
    return mousePos.x >= spritePos.x && mousePos.x <= spritePos.x + spriteSize.x &&
           mousePos.y >= spritePos.y && mousePos.y <= spritePos.y + spriteSize.y;
}
