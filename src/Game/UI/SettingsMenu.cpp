#include "SettingsMenu.h"
#include "BitmapFont.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Input/Input.h"
#include "../../Engine/Audio/AudioManager.h"
#include "../../Engine/Utility/WinApp.h"
#include "../GameObject/FPSCamera.h"
#include "UnoEngine.h"
#include <algorithm>
#include <cmath>

SettingsMenu::SettingsMenu() = default;
SettingsMenu::~SettingsMenu() = default;

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

    // Dividers (3: under title, under sensitivity, under volume)
    float dividerYs[] = {175.0f, 290.0f, 420.0f};
    for (int i = 0; i < 3; ++i) {
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

    // Close icon
    closeIconSprite_ = std::make_unique<Sprite>();
    closeIconSprite_->Initialize(spriteCommon_, uiPath + "close_icon.png");
    closeIconSprite_->SetPosition({PANEL_X + PANEL_W - 50.0f, PANEL_Y + 10.0f});
    closeIconSprite_->SetSize({32.0f, 32.0f});

    // BitmapFont (おそろしげ明朝)
    bitmapFont_ = std::make_unique<BitmapFont>();
    bitmapFont_->Initialize(spriteCommon_, "Resources/font/osoro.fnt");

    // Read current fullscreen state
    auto* winApp = UnoEngine::GetInstance()->GetWinApp();
    if (winApp) {
        isFullscreen_ = winApp->IsFullscreen();
    }
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
}

void SettingsMenu::Close() {
    if (!isOpen_) return;
    isOpen_ = false;
    draggingSlider_ = -1;

    // Hide cursor and reset mouse center
    if (input_) {
        input_->SetMouseCursor(false);
        input_->ResetMouseCenter();
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

    // Update knob positions based on current values
    float sensNorm = (mouseSensitivity_ - SENS_MIN) / (SENS_MAX - SENS_MIN);
    float sensKnobX = SLIDER_X + sensNorm * SLIDER_W - KNOB_SIZE * 0.5f;
    float sensKnobY = SENSITIVITY_SLIDER_Y - KNOB_SIZE * 0.5f + SLIDER_H * 0.5f;
    sliderKnobSprites_[0]->SetPosition({sensKnobX, sensKnobY});

    float volKnobX = SLIDER_X + masterVolume_ * SLIDER_W - KNOB_SIZE * 0.5f;
    float volKnobY = VOLUME_SLIDER_Y - KNOB_SIZE * 0.5f + SLIDER_H * 0.5f;
    sliderKnobSprites_[1]->SetPosition({volKnobX, volKnobY});
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

    // Window mode buttons
    for (int i = 0; i < 2; ++i) {
        bool selected = (i == 0) ? isFullscreen_ : !isFullscreen_;
        if (selected) {
            buttonSelectedSprites_[i]->setColor({1.0f, 1.0f, 1.0f, a});
            buttonSelectedSprites_[i]->Update();
            buttonSelectedSprites_[i]->Draw();
        } else {
            buttonNormalSprites_[i]->setColor({1.0f, 1.0f, 1.0f, a});
            buttonNormalSprites_[i]->Update();
            buttonNormalSprites_[i]->Draw();
        }
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

        // Hint (centered)
        float hintW = bitmapFont_->MeasureTextWidth(L"ESC: 閉じる", HINT_SCALE);
        bitmapFont_->RenderText(L"ESC: 閉じる",
            {PANEL_X + (PANEL_W - hintW) * 0.5f, PANEL_Y + PANEL_H - 40.0f}, HINT_SCALE, hintColor);
    }
}

void SettingsMenu::ApplySettings() {
    // Mouse sensitivity
    if (fpsCamera_) {
        fpsCamera_->SetMouseSensitivity(mouseSensitivity_);
    }

    // Master volume
    AudioManager::GetInstance()->SetMasterVolume(masterVolume_);
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
