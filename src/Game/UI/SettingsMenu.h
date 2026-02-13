#pragma once
#include <memory>
#include <array>

class SpriteCommon;
class Sprite;
class BitmapFont;
class Input;
class FPSCamera;

class SettingsMenu {
public:
    SettingsMenu();
    ~SettingsMenu();

    void Initialize(SpriteCommon* spriteCommon, Input* input);
    void Update(float deltaTime);
    void Draw();

    void Open();
    void Close();
    bool IsOpen() const { return isOpen_; }

    void SetFPSCamera(FPSCamera* camera) { fpsCamera_ = camera; }

private:
    void ApplySettings();
    float GetMouseX() const;
    float GetMouseY() const;
    bool IsMouseDown() const;
    bool IsMouseClicked() const;
    bool IsPointInRect(float px, float py, float rx, float ry, float rw, float rh) const;

    bool isOpen_ = false;
    SpriteCommon* spriteCommon_ = nullptr;
    Input* input_ = nullptr;
    FPSCamera* fpsCamera_ = nullptr;

    // Sprites
    std::unique_ptr<Sprite> bgSprite_;
    std::array<std::unique_ptr<Sprite>, 2> sliderTrackSprites_;
    std::array<std::unique_ptr<Sprite>, 2> sliderKnobSprites_;
    std::array<std::unique_ptr<Sprite>, 3> dividerSprites_;
    std::array<std::unique_ptr<Sprite>, 2> buttonNormalSprites_;
    std::array<std::unique_ptr<Sprite>, 2> buttonSelectedSprites_;
    std::unique_ptr<Sprite> closeIconSprite_;

    // BitmapFont for text
    std::unique_ptr<BitmapFont> bitmapFont_;

    // Settings values
    float mouseSensitivity_ = 0.003f;   // 0.001 ~ 0.01
    float masterVolume_ = 0.25f;        // 0.0 ~ 1.0
    bool isFullscreen_ = false;

    // Slider interaction state
    int draggingSlider_ = -1; // -1: none, 0: sensitivity, 1: volume

    // Layout constants
    static constexpr float PANEL_X = 290.0f;
    static constexpr float PANEL_Y = 80.0f;
    static constexpr float PANEL_W = 700.0f;
    static constexpr float PANEL_H = 560.0f;

    static constexpr float SLIDER_X = 340.0f;
    static constexpr float SLIDER_W = 400.0f;
    static constexpr float SLIDER_H = 8.0f;
    static constexpr float KNOB_SIZE = 24.0f;

    static constexpr float SENSITIVITY_SLIDER_Y = 230.0f;
    static constexpr float VOLUME_SLIDER_Y = 360.0f;

    static constexpr float BUTTON_W = 180.0f;
    static constexpr float BUTTON_H = 44.0f;
    static constexpr float BUTTON_Y = 490.0f;
    static constexpr float BUTTON1_X = 400.0f;
    static constexpr float BUTTON2_X = 620.0f;

    // Sensitivity range
    static constexpr float SENS_MIN = 0.001f;
    static constexpr float SENS_MAX = 0.01f;
};
