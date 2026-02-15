#include "GameOverScene.h"
#include "../../Engine/Resource/ResourcePreloader.h"
#include "SceneManager.h"
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

    // TVノイズのみ - 常に最大強度で表示
    if (horrorEffect_) {
        horrorEffect_->SetHorrorParams(
            0.0f,   // time
            1.0f,   // noiseIntensity（最大）
            0.3f,   // distortionIntensity（軽め）
            0.0f,   // bloodAmount
            0.5f    // vignetteIntensity
        );
    }

    // ゲームオーバーテキストスプライトのみ初期化
    gameOverTextSprite_ = std::make_unique<Sprite>();
    gameOverTextSprite_->Initialize(spriteCommon_, "Resources/textures/Title/Title_moji.png");
    gameOverTextSprite_->SetPosition({640.0f, 360.0f});
    gameOverTextSprite_->SetAnchorPoint({0.5f, 0.5f});

    retrySprite_ = std::make_unique<Sprite>();
    retrySprite_->Initialize(spriteCommon_, "Resources/textures/Title/hazimeru.png");
    retryOriginalSize_ = retrySprite_->GetSize();
    retrySprite_->SetPosition({640.0f, 500.0f});
    retrySprite_->SetAnchorPoint({0.5f, 0.5f});

    titleSprite_ = std::make_unique<Sprite>();
    titleSprite_->Initialize(spriteCommon_, "Resources/textures/Title/owaru.png");
    titleOriginalSize_ = titleSprite_->GetSize();
    titleSprite_->SetPosition({640.0f, 590.0f});
    titleSprite_->SetAnchorPoint({0.5f, 0.5f});

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
            if (prevW != 0 && horrorEffect_) {
                horrorEffect_->ResizeRenderTarget();
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

    // TVノイズのパラメータを更新（時間だけ）
    if (horrorEffect_) {
        horrorEffect_->SetHorrorParams(
            time_,
            1.0f,   // noiseIntensity（常に最大）
            0.3f,   // distortionIntensity
            0.0f,   // bloodAmount
            0.5f    // vignetteIntensity
        );
    }

    // キーボード入力フラグ
    bool keyPressed = false;

    // W/上矢印キーでメニュー選択を上に
    if (input_->TriggerKey(DIK_W) || input_->TriggerKey(DIK_UP)) {
        currentSelection_ = MenuSelection::Retry;
        keyPressed = true;
    }
    // S/下矢印キーでメニュー選択を下に
    if (input_->TriggerKey(DIK_S) || input_->TriggerKey(DIK_DOWN)) {
        currentSelection_ = MenuSelection::Title;
        keyPressed = true;
    }

    // マウスでホバー検知（キーが押されていない時のみ）
    bool retryHovered = false;
    bool titleHovered = false;
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
        Vector2 retryPos = retrySprite_->GetPosition();
        Vector2 retryMin = { retryPos.x - retryOriginalSize_.x * 0.5f, retryPos.y - retryOriginalSize_.y * 0.5f };
        Vector2 retryMax = { retryPos.x + retryOriginalSize_.x * 0.5f, retryPos.y + retryOriginalSize_.y * 0.5f };

        Vector2 titlePos = titleSprite_->GetPosition();
        Vector2 titleMin = { titlePos.x - titleOriginalSize_.x * 0.5f, titlePos.y - titleOriginalSize_.y * 0.5f };
        Vector2 titleMax = { titlePos.x + titleOriginalSize_.x * 0.5f, titlePos.y + titleOriginalSize_.y * 0.5f };

        if (mousePos.x >= retryMin.x && mousePos.x <= retryMax.x &&
            mousePos.y >= retryMin.y && mousePos.y <= retryMax.y) {
            retryHovered = true;
            currentSelection_ = MenuSelection::Retry;
        }
        if (mousePos.x >= titleMin.x && mousePos.x <= titleMax.x &&
            mousePos.y >= titleMin.y && mousePos.y <= titleMax.y) {
            titleHovered = true;
            currentSelection_ = MenuSelection::Title;
        }
    }

    // 選択状態に応じた視覚フィードバック
    if (currentSelection_ == MenuSelection::Retry || retryHovered) {
        retrySprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        titleSprite_->setColor({ 0.5f, 0.5f, 0.5f, 1.0f });
    } else {
        titleSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        retrySprite_->setColor({ 0.5f, 0.5f, 0.5f, 1.0f });
    }

    // スプライトの更新
    gameOverTextSprite_->Update();
    retrySprite_->Update();
    titleSprite_->Update();


    // マウスクリックで決定
    DIMOUSESTATE mouseState;
    if (SUCCEEDED(input_->GetMouseState(&mouseState))) {
        if (mouseState.rgbButtons[0] & 0x80) { // 左クリック
            if (retryHovered) {
                sceneManager_->ChangeScene("GamePlay");
            }
            if (titleHovered) {
                sceneManager_->ChangeScene("Title");
            }
        }
    }

    // SPACEまたはENTERで決定
    if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
        if (currentSelection_ == MenuSelection::Retry) {
            sceneManager_->ChangeScene("GamePlay");
        } else {
            sceneManager_->ChangeScene("Title");
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

    // テキストとメニューのみ描画
    if (gameOverTextSprite_) gameOverTextSprite_->Draw();
    if (retrySprite_) retrySprite_->Draw();
    if (titleSprite_) titleSprite_->Draw();

    // TVノイズを適用して画面に描画
    if (horrorEffect_) {
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

    gameOverTextSprite_.reset();
    retrySprite_.reset();
    titleSprite_.reset();
}

bool GameOverScene::CheckMouseHover(const Vector2& mousePos, const Vector2& spritePos, const Vector2& spriteSize) {
    return mousePos.x >= spritePos.x && mousePos.x <= spritePos.x + spriteSize.x &&
           mousePos.y >= spritePos.y && mousePos.y <= spritePos.y + spriteSize.y;
}
