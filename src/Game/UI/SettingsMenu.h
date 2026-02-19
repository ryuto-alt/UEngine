#pragma once
#include <memory>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>

class SpriteCommon;
class Sprite;
class BitmapFont;
class Input;
class FPSCamera;

class SettingsMenu {
public:
    SettingsMenu();
    ~SettingsMenu();

    // シーンをまたいで設定を保持する静的ストレージ
    struct SavedSettings {
        float mouseSensitivity = 0.003f;
        float masterVolume     = 1.0f;
    };
    static SavedSettings s_saved;

    void Initialize(SpriteCommon* spriteCommon, Input* input);
    void Update(float deltaTime);
    void Draw();

    void Open();
    void Close();
    bool IsOpen() const { return isOpen_; }

    void SetFPSCamera(FPSCamera* camera) { fpsCamera_ = camera; }

    // BGMキー登録（設定メニュー中にフェードアウト→一時停止、閉じたら再開）
    void AddBGMKey(const std::string& key);

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
    std::array<std::unique_ptr<Sprite>, 4> dividerSprites_;
    std::array<std::unique_ptr<Sprite>, 2> buttonNormalSprites_;
    std::array<std::unique_ptr<Sprite>, 2> buttonSelectedSprites_;
    std::unique_ptr<Sprite> exitButtonSprite_;
    std::unique_ptr<Sprite> closeIconSprite_;

    // BitmapFont for text
    std::unique_ptr<BitmapFont> bitmapFont_;

    // Settings values
    float mouseSensitivity_ = 0.003f;   // 0.001 ~ 0.01
    float masterVolume_ = 1.0f;        // 0.0 ~ 1.0 (実際のXAudio2音量は x2倍)
    bool isFullscreen_ = false;

    // Slider interaction state
    int draggingSlider_ = -1; // -1: none, 0: sensitivity, 1: volume

    // Button hover animation (0.0 ~ 1.0, smoothly interpolated)
    float hoverFullscreenT_ = 0.0f;
    float hoverWindowedT_ = 0.0f;
    float hoverExitT_ = 0.0f;
    static constexpr float HOVER_SPEED = 8.0f;
    static constexpr float HOVER_SCALE = 1.06f;

    // Fade-in
    float fadeAlpha_ = 0.0f;
    static constexpr float FADE_DURATION = 0.15f;

    // Layout constants
    static constexpr float PANEL_X = 290.0f;
    static constexpr float PANEL_Y = 80.0f;
    static constexpr float PANEL_W = 700.0f;
    static constexpr float PANEL_H = 630.0f;

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

    // Exit button (centered with window mode button group)
    static constexpr float EXIT_BUTTON_X = (BUTTON1_X + BUTTON2_X + BUTTON_W) * 0.5f - BUTTON_W * 0.5f;
    static constexpr float EXIT_BUTTON_Y = 560.0f;

    // Sensitivity range
    static constexpr float SENS_MIN = 0.001f;
    static constexpr float SENS_MAX = 0.01f;

    // BGM fade on pause
    std::vector<std::string> bgmKeys_;
    std::unordered_map<std::string, float> savedBGMVolumes_;
    float bgmFadeTimer_ = 0.0f;
    bool bgmFadingOut_ = false;
    bool bgmPaused_ = false;
    static constexpr float BGM_FADE_DURATION = 0.4f;
};
