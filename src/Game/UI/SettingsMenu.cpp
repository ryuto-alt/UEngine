#include "SettingsMenu.h"
#include "BitmapFont.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Input/Input.h"
#include "../../Engine/Audio/AudioManager.h"
#include "../../Engine/Utility/WinApp.h"
#include "../GameObject/FPSCamera.h"
#include "../scene/SceneManager.h"
#include "UnoEngine.h"
#include <algorithm>
#include <cmath>

SettingsMenu::SettingsMenu() = default;
SettingsMenu::~SettingsMenu() = default;

SettingsMenu::SavedSettings SettingsMenu::s_saved;

void SettingsMenu::Initialize(SpriteCommon* spriteCommon, Input* input) {
    spriteCommon_ = spriteCommon;
    input_ = input;

    const std::string uiPath = "Resources/textures/UI/";

    // Background overlay
    bgSprite_ = std::make_unique<Sprite>();
    bgSprite_->Initialize(spriteCommon_, uiPath + "settings_bg.png");
    bgSprite_->SetPosition({PANEL_X, PANEL_Y});
    bgSprite_->SetSize({PANEL_W, PANEL_H});

    // Slider tracks
    for (int i = 0; i < 2; ++i) {
        sliderTrackSprites_[i] = std::make_unique<Sprite>();
        sliderTrackSprites_[i]->Initialize(spriteCommon_, uiPath + "slider_track.png");
        float y = (i == 0) ? SENSITIVITY_SLIDER_Y : VOLUME_SLIDER_Y;
        sliderTrackSprites_[i]->SetPosition({SLIDER_X, y});
        sliderTrackSprites_[i]->SetSize({SLIDER_W, SLIDER_H});
    }

    // Slider knobs
    for (int i = 0; i < 2; ++i) {
        sliderKnobSprites_[i] = std::make_unique<Sprite>();
        sliderKnobSprites_[i]->Initialize(spriteCommon_, uiPath + "slider_knob.png");
        sliderKnobSprites_[i]->SetSize({KNOB_SIZE, KNOB_SIZE});
    }

    // Dividers (4: under title, under sensitivity, under volume, under window mode)
    float dividerYs[] = {175.0f, 290.0f, 420.0f, 545.0f};
    for (int i = 0; i < 4; ++i) {
        dividerSprites_[i] = std::make_unique<Sprite>();
        dividerSprites_[i]->Initialize(spriteCommon_, uiPath + "divider.png");
        dividerSprites_[i]->SetPosition({PANEL_X + 20.0f, PANEL_Y + dividerYs[i] - PANEL_Y});
        dividerSprites_[i]->SetSize({PANEL_W - 40.0f, 2.0f});
    }

    // Window mode buttons
    for (int i = 0; i < 2; ++i) {
        buttonNormalSprites_[i] = std::make_unique<Sprite>();
        buttonNormalSprites_[i]->Initialize(spriteCommon_, uiPath + "button_normal.png");
        float x = (i == 0) ? BUTTON1_X : BUTTON2_X;
        buttonNormalSprites_[i]->SetPosition({x, BUTTON_Y});
        buttonNormalSprites_[i]->SetSize({BUTTON_W, BUTTON_H});

        buttonSelectedSprites_[i] = std::make_unique<Sprite>();
        buttonSelectedSprites_[i]->Initialize(spriteCommon_, uiPath + "button_selected.png");
        buttonSelectedSprites_[i]->SetPosition({x, BUTTON_Y});
        buttonSelectedSprites_[i]->SetSize({BUTTON_W, BUTTON_H});
    }

    // Exit button (centered with window mode buttons)
    exitButtonSprite_ = std::make_unique<Sprite>();
    exitButtonSprite_->Initialize(spriteCommon_, uiPath + "button_normal.png");
    exitButtonSprite_->SetPosition({EXIT_BUTTON_X, EXIT_BUTTON_Y});
    exitButtonSprite_->SetSize({BUTTON_W, BUTTON_H});

    // Close icon
    closeIconSprite_ = std::make_unique<Sprite>();
    closeIconSprite_->Initialize(spriteCommon_, uiPath + "close_icon.png");
    closeIconSprite_->SetPosition({PANEL_X + PANEL_W - 50.0f, PANEL_Y + 10.0f});
    closeIconSprite_->SetSize({32.0f, 32.0f});

    // BitmapFont (おそろしげ明朝)
    bitmapFont_ = std::make_unique<BitmapFont>();
    bitmapFont_->Initialize(spriteCommon_, "Resources/fonts/osoro.fnt");

    // Read current fullscreen state
    auto* winApp = UnoEngine::GetInstance()->GetWinApp();
    if (winApp) {
        isFullscreen_ = winApp->IsFullscreen();
    }

    // 保存済み設定を復元して即適用（シーン再生成後もリセットされない）
    mouseSensitivity_ = s_saved.mouseSensitivity;
    masterVolume_     = s_saved.masterVolume;
    ApplySettings();
}

void SettingsMenu::Open() {
    if (isOpen_) return;
    isOpen_ = true;
    draggingSlider_ = -1;
    fadeAlpha_ = 0.0f;

    // Read current values
    if (fpsCamera_) {
        mouseSensitivity_ = fpsCamera_->GetMouseSensitivity();
    }

    // Show mouse cursor
    if (input_) {
        input_->SetMouseCursor(true);
    }

    // BGMフェードアウト開始
    if (!bgmKeys_.empty()) {
        auto* audio = AudioManager::GetInstance();
        savedBGMVolumes_.clear();
        for (auto& key : bgmKeys_) {
            savedBGMVolumes_[key] = audio->GetVolume(key);
        }
        bgmFadingOut_ = true;
        bgmFadeTimer_ = 0.0f;
        bgmPaused_ = false;
    }
}

void SettingsMenu::Close() {
    if (!isOpen_) return;
    isOpen_ = false;
    draggingSlider_ = -1;

    if (input_) {
        // FPSCamera使用時のみカーソルを非表示（ゲームプレイ画面）
        if (fpsCamera_) {
            input_->SetMouseCursor(false);
        }
        input_->ResetMouseCenter();
    }

    // BGM再開（保存した音量に復元）
    if (bgmPaused_ || bgmFadingOut_) {
        auto* audio = AudioManager::GetInstance();
        for (auto& key : bgmKeys_) {
            audio->Resume(key);
            if (savedBGMVolumes_.count(key)) {
                audio->SetVolume(key, savedBGMVolumes_[key]);
            }
        }
        bgmPaused_ = false;
        bgmFadingOut_ = false;
    }
}

void SettingsMenu::Update(float deltaTime) {
    if (!isOpen_) return;

    // Fade-in
    if (fadeAlpha_ < 1.0f) {
        fadeAlpha_ += deltaTime / FADE_DURATION;
        if (fadeAlpha_ > 1.0f) fadeAlpha_ = 1.0f;
    }

    float mx = GetMouseX();
    float my = GetMouseY();
    bool clicked = IsMouseClicked();
    bool held = IsMouseDown();

    // Close button click
    if (clicked && IsPointInRect(mx, my,
        PANEL_X + PANEL_W - 50.0f, PANEL_Y + 10.0f, 32.0f, 32.0f)) {
        Close();
        return;
    }

    // --- Slider interaction ---

    // Start dragging on click
    if (clicked) {
        // Check sensitivity slider knob/track area
        float sensNorm = (mouseSensitivity_ - SENS_MIN) / (SENS_MAX - SENS_MIN);
        float sensKnobX = SLIDER_X + sensNorm * SLIDER_W - KNOB_SIZE * 0.5f;
        float sensKnobY = SENSITIVITY_SLIDER_Y - KNOB_SIZE * 0.5f + SLIDER_H * 0.5f;

        if (IsPointInRect(mx, my, SLIDER_X - KNOB_SIZE * 0.5f,
            SENSITIVITY_SLIDER_Y - KNOB_SIZE, SLIDER_W + KNOB_SIZE, KNOB_SIZE * 2.0f + SLIDER_H)) {
            draggingSlider_ = 0;
        }

        // Check volume slider
        float volKnobX = SLIDER_X + masterVolume_ * SLIDER_W - KNOB_SIZE * 0.5f;
        float volKnobY = VOLUME_SLIDER_Y - KNOB_SIZE * 0.5f + SLIDER_H * 0.5f;

        if (IsPointInRect(mx, my, SLIDER_X - KNOB_SIZE * 0.5f,
            VOLUME_SLIDER_Y - KNOB_SIZE, SLIDER_W + KNOB_SIZE, KNOB_SIZE * 2.0f + SLIDER_H)) {
            draggingSlider_ = 1;
        }
    }

    // Dragging
    if (held && draggingSlider_ >= 0) {
        float norm = (mx - SLIDER_X) / SLIDER_W;
        norm = (std::max)(0.0f, (std::min)(1.0f, norm));

        if (draggingSlider_ == 0) {
            mouseSensitivity_ = SENS_MIN + norm * (SENS_MAX - SENS_MIN);
        } else {
            masterVolume_ = norm;
        }
        ApplySettings();
    }

    // Stop dragging on release
    if (!held) {
        draggingSlider_ = -1;
    }

    // --- Smooth hover transitions ---
    {
        auto lerp = [&](float& t, bool hovered) {
            float target = hovered ? 1.0f : 0.0f;
            t += (target - t) * (std::min)(1.0f, HOVER_SPEED * deltaTime);
            t = (std::max)(0.0f, (std::min)(1.0f, t));
        };
        lerp(hoverFullscreenT_, IsPointInRect(mx, my, BUTTON1_X, BUTTON_Y, BUTTON_W, BUTTON_H));
        lerp(hoverWindowedT_, IsPointInRect(mx, my, BUTTON2_X, BUTTON_Y, BUTTON_W, BUTTON_H));
        lerp(hoverExitT_, IsPointInRect(mx, my, EXIT_BUTTON_X, EXIT_BUTTON_Y, BUTTON_W, BUTTON_H));
    }

    // --- Window mode buttons ---
    if (clicked) {
        // Fullscreen button
        if (IsPointInRect(mx, my, BUTTON1_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
            if (!isFullscreen_) {
                isFullscreen_ = true;
                auto* engine = UnoEngine::GetInstance();
                auto* winApp = engine->GetWinApp();
                if (winApp) {
                    winApp->ToggleFullscreen();
                    uint32_t w = winApp->GetCurrentWindowWidth();
                    uint32_t h = winApp->GetCurrentWindowHeight();
                    engine->GetDXCom()->ResizeBuffers(w, h);
                }
                if (input_) input_->UpdateWindowCenter();
            }
        }
        // Windowed button
        if (IsPointInRect(mx, my, BUTTON2_X, BUTTON_Y, BUTTON_W, BUTTON_H)) {
            if (isFullscreen_) {
                isFullscreen_ = false;
                auto* engine = UnoEngine::GetInstance();
                auto* winApp = engine->GetWinApp();
                if (winApp) {
                    winApp->ToggleFullscreen();
                    uint32_t w = winApp->GetCurrentWindowWidth();
                    uint32_t h = winApp->GetCurrentWindowHeight();
                    engine->GetDXCom()->ResizeBuffers(w, h);
                }
                if (input_) input_->UpdateWindowCenter();
            }
        }
    }

    // --- Exit button ---
    if (clicked) {
        if (IsPointInRect(mx, my, EXIT_BUTTON_X, EXIT_BUTTON_Y, BUTTON_W, BUTTON_H)) {
            Close();
            SceneManager::GetInstance()->RequestExit();
            return;
        }
    }

    // Update knob positions based on current values
    float sensNorm = (mouseSensitivity_ - SENS_MIN) / (SENS_MAX - SENS_MIN);
    float sensKnobX = SLIDER_X + sensNorm * SLIDER_W - KNOB_SIZE * 0.5f;
    float sensKnobY = SENSITIVITY_SLIDER_Y - KNOB_SIZE * 0.5f + SLIDER_H * 0.5f;
    sliderKnobSprites_[0]->SetPosition({sensKnobX, sensKnobY});

    float volKnobX = SLIDER_X + masterVolume_ * SLIDER_W - KNOB_SIZE * 0.5f;
    float volKnobY = VOLUME_SLIDER_Y - KNOB_SIZE * 0.5f + SLIDER_H * 0.5f;
    sliderKnobSprites_[1]->SetPosition({volKnobX, volKnobY});

    // --- BGM fade out ---
    if (bgmFadingOut_ && !bgmPaused_) {
        bgmFadeTimer_ += deltaTime;
        float t = bgmFadeTimer_ / BGM_FADE_DURATION;
        auto* audio = AudioManager::GetInstance();
        if (t >= 1.0f) {
            // フェード完了 → 一時停止
            for (auto& key : bgmKeys_) {
                audio->SetVolume(key, 0.0f);
                audio->Pause(key);
            }
            bgmFadingOut_ = false;
            bgmPaused_ = true;
        } else {
            // フェード中
            for (auto& key : bgmKeys_) {
                float saved = savedBGMVolumes_.count(key) ? savedBGMVolumes_[key] : 0.0f;
                audio->SetVolume(key, saved * (1.0f - t));
            }
        }
    }
}

void SettingsMenu::Draw() {
    if (!isOpen_ || !spriteCommon_) return;

    float a = fadeAlpha_;

    spriteCommon_->CommonDraw();

    // Background
    bgSprite_->setColor({1.0f, 1.0f, 1.0f, a});
    bgSprite_->Update();
    bgSprite_->Draw();

    // Dividers
    for (auto& div : dividerSprites_) {
        div->setColor({1.0f, 1.0f, 1.0f, a});
        div->Update();
        div->Draw();
    }

    // Slider tracks
    for (auto& track : sliderTrackSprites_) {
        track->setColor({1.0f, 1.0f, 1.0f, a});
        track->Update();
        track->Draw();
    }

    // Slider knobs
    for (auto& knob : sliderKnobSprites_) {
        knob->setColor({1.0f, 1.0f, 1.0f, a});
        knob->Update();
        knob->Draw();
    }

    // Window mode buttons (smooth hover: brightness + scale)
    for (int i = 0; i < 2; ++i) {
        bool selected = (i == 0) ? isFullscreen_ : !isFullscreen_;
        float t = (i == 0) ? hoverFullscreenT_ : hoverWindowedT_;
        float origX = (i == 0) ? BUTTON1_X : BUTTON2_X;

        float scale = 1.0f + (HOVER_SCALE - 1.0f) * t;
        float sw = BUTTON_W * scale;
        float sh = BUTTON_H * scale;
        float sx = origX - (sw - BUTTON_W) * 0.5f;
        float sy = BUTTON_Y - (sh - BUTTON_H) * 0.5f;

        Sprite* sprite = selected ? buttonSelectedSprites_[i].get() : buttonNormalSprites_[i].get();
        float b = selected ? 1.0f : (0.55f + 0.45f * t);
        sprite->SetPosition({sx, sy});
        sprite->SetSize({sw, sh});
        sprite->setColor({b, b, b, a});
        sprite->Update();
        sprite->Draw();
        // 元サイズに戻す（他フレームへの影響防止）
        sprite->SetPosition({origX, BUTTON_Y});
        sprite->SetSize({BUTTON_W, BUTTON_H});
    }

    // Exit button (smooth hover: red brightness + scale)
    {
        float t = hoverExitT_;
        float scale = 1.0f + (HOVER_SCALE - 1.0f) * t;
        float sw = BUTTON_W * scale;
        float sh = BUTTON_H * scale;
        float sx = EXIT_BUTTON_X - (sw - BUTTON_W) * 0.5f;
        float sy = EXIT_BUTTON_Y - (sh - BUTTON_H) * 0.5f;

        float r = 0.65f + 0.35f * t;
        float g = 0.22f + 0.28f * t;
        float b = 0.22f + 0.28f * t;
        exitButtonSprite_->SetPosition({sx, sy});
        exitButtonSprite_->SetSize({sw, sh});
        exitButtonSprite_->setColor({r, g, b, a});
        exitButtonSprite_->Update();
        exitButtonSprite_->Draw();
        exitButtonSprite_->SetPosition({EXIT_BUTTON_X, EXIT_BUTTON_Y});
        exitButtonSprite_->SetSize({BUTTON_W, BUTTON_H});
    }

    // Close icon
    closeIconSprite_->setColor({1.0f, 1.0f, 1.0f, a});
    closeIconSprite_->Update();
    closeIconSprite_->Draw();

    // --- Text rendering ---
    // osoro.fnt = 32px base font. Scale values are relative to that.
    if (bitmapFont_) {
        bitmapFont_->BeginDraw();
        constexpr float TITLE_SCALE = 0.85f;   // ~27px
        constexpr float LABEL_SCALE = 0.55f;   // ~18px
        constexpr float VALUE_SCALE = 0.55f;   // ~18px
        constexpr float BTN_SCALE   = 0.45f;   // ~14px
        constexpr float HINT_SCALE  = 0.45f;   // ~14px

        Vector4 textColor = {1.0f, 1.0f, 1.0f, a};
        Vector4 hintColor = {0.7f, 0.7f, 0.7f, a};

        // Title (centered)
        float titleW = bitmapFont_->MeasureTextWidth(L"設定", TITLE_SCALE);
        bitmapFont_->RenderText(L"設定",
            {PANEL_X + (PANEL_W - titleW) * 0.5f, PANEL_Y + 18.0f}, TITLE_SCALE, textColor);

        // Sensitivity label + value
        float sensPercent = (mouseSensitivity_ - SENS_MIN) / (SENS_MAX - SENS_MIN) * 100.0f;
        bitmapFont_->RenderText(L"マウス感度", {SLIDER_X, SENSITIVITY_SLIDER_Y - 35.0f}, LABEL_SCALE, textColor);
        wchar_t sensBuf[16];
        swprintf_s(sensBuf, L"%.0f%%", sensPercent);
        bitmapFont_->RenderText(sensBuf, {SLIDER_X + SLIDER_W + 15.0f, SENSITIVITY_SLIDER_Y - 6.0f}, VALUE_SCALE, textColor);

        // Volume label + value
        float volPercent = masterVolume_ * 100.0f;
        bitmapFont_->RenderText(L"音量", {SLIDER_X, VOLUME_SLIDER_Y - 35.0f}, LABEL_SCALE, textColor);
        wchar_t volBuf[16];
        swprintf_s(volBuf, L"%.0f%%", volPercent);
        bitmapFont_->RenderText(volBuf, {SLIDER_X + SLIDER_W + 15.0f, VOLUME_SLIDER_Y - 6.0f}, VALUE_SCALE, textColor);

        // Window mode label
        bitmapFont_->RenderText(L"ウィンドウモード", {SLIDER_X, BUTTON_Y - 35.0f}, LABEL_SCALE, textColor);

        // Button labels (centered in buttons)
        float fs_w = bitmapFont_->MeasureTextWidth(L"フルスクリーン", BTN_SCALE);
        float win_w = bitmapFont_->MeasureTextWidth(L"ウィンドウ", BTN_SCALE);
        bitmapFont_->RenderText(L"フルスクリーン",
            {BUTTON1_X + (BUTTON_W - fs_w) * 0.5f, BUTTON_Y + (BUTTON_H - 32.0f * BTN_SCALE) * 0.5f}, BTN_SCALE, textColor);
        bitmapFont_->RenderText(L"ウィンドウ",
            {BUTTON2_X + (BUTTON_W - win_w) * 0.5f, BUTTON_Y + (BUTTON_H - 32.0f * BTN_SCALE) * 0.5f}, BTN_SCALE, textColor);

        // Exit button label (centered)
        float exit_w = bitmapFont_->MeasureTextWidth(L"ゲーム終了", BTN_SCALE);
        bitmapFont_->RenderText(L"ゲーム終了",
            {EXIT_BUTTON_X + (BUTTON_W - exit_w) * 0.5f, EXIT_BUTTON_Y + (BUTTON_H - 32.0f * BTN_SCALE) * 0.5f}, BTN_SCALE, textColor);

        // Hint (centered)
        float hintW = bitmapFont_->MeasureTextWidth(L"ESC: 閉じる", HINT_SCALE);
        bitmapFont_->RenderText(L"ESC: 閉じる",
            {PANEL_X + (PANEL_W - hintW) * 0.5f, PANEL_Y + PANEL_H - 40.0f}, HINT_SCALE, hintColor);
    }
}

void SettingsMenu::AddBGMKey(const std::string& key) {
    bgmKeys_.push_back(key);
}

void SettingsMenu::ApplySettings() {
    // Mouse sensitivity
    if (fpsCamera_) {
        fpsCamera_->SetMouseSensitivity(mouseSensitivity_);
    }

    // Master volume
    AudioManager::GetInstance()->SetMasterVolume(masterVolume_);

    // シーンをまたいで設定を保持
    s_saved.mouseSensitivity = mouseSensitivity_;
    s_saved.masterVolume     = masterVolume_;
}

float SettingsMenu::GetMouseX() const {
    POINT pt;
    GetCursorPos(&pt);
    HWND hwnd = UnoEngine::GetInstance()->GetWinApp()->GetHwnd();
    ScreenToClient(hwnd, &pt);
    // フルスクリーン時はクライアント座標を論理座標(1280x720)にスケーリング
    RECT rc;
    GetClientRect(hwnd, &rc);
    float clientW = static_cast<float>(rc.right - rc.left);
    return static_cast<float>(pt.x) * (static_cast<float>(WinApp::kClientWidth) / clientW);
}

float SettingsMenu::GetMouseY() const {
    POINT pt;
    GetCursorPos(&pt);
    HWND hwnd = UnoEngine::GetInstance()->GetWinApp()->GetHwnd();
    ScreenToClient(hwnd, &pt);
    RECT rc;
    GetClientRect(hwnd, &rc);
    float clientH = static_cast<float>(rc.bottom - rc.top);
    return static_cast<float>(pt.y) * (static_cast<float>(WinApp::kClientHeight) / clientH);
}

bool SettingsMenu::IsMouseDown() const {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

bool SettingsMenu::IsMouseClicked() const {
    if (input_) {
        return input_->IsMouseButtonTriggered(0);
    }
    return false;
}

bool SettingsMenu::IsPointInRect(float px, float py, float rx, float ry, float rw, float rh) const {
    return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}
