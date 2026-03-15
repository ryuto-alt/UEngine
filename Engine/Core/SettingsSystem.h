#pragma once

#include "NonCopyable.h"
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace UnoEngine {

// グラフィックス設定
struct GraphicsSettings {
    uint32_t resolutionWidth = 1280;
    uint32_t resolutionHeight = 720;
    bool fullscreen = false;
    bool vsync = true;
    int32_t qualityLevel = 2;  // 0=Low, 1=Medium, 2=High
    float renderScale = 1.0f;
};

// オーディオ設定
struct AudioSettings {
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 1.0f;
    bool muteAll = false;
};

// 入力設定
struct InputSettings {
    float mouseSensitivity = 1.0f;
    bool invertY = false;
    float gamepadSensitivity = 1.0f;
    float gamepadDeadzone = 0.24f;
};

// ゲーム設定
struct GameSettings {
    std::string language = "ja";
    bool showFPS = false;
    float fieldOfView = 70.0f;
};

// 設定システム - シングルトン
class SettingsSystem : public NonCopyable {
public:
    ~SettingsSystem() = default;

    // シングルトンアクセス
    static SettingsSystem& GetInstance();

    // ファイルI/O
    bool Load(const std::string& filepath = "settings.json");
    bool Save(const std::string& filepath = "settings.json") const;

    // デフォルト値にリセット
    void ResetToDefaults();

    // アクセサ（const / non-const）
    GraphicsSettings& GetGraphics() { return graphics_; }
    const GraphicsSettings& GetGraphics() const { return graphics_; }
    AudioSettings& GetAudio() { return audio_; }
    const AudioSettings& GetAudio() const { return audio_; }
    InputSettings& GetInput() { return input_; }
    const InputSettings& GetInput() const { return input_; }
    GameSettings& GetGame() { return game_; }
    const GameSettings& GetGame() const { return game_; }

    // サブシステムへ設定を適用
    void ApplyAll();

    // 設定変更イベントを発火
    void NotifyChanged();

private:
    SettingsSystem() = default;

    // JSON変換
    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);

    GraphicsSettings graphics_;
    AudioSettings audio_;
    InputSettings input_;
    GameSettings game_;
};

} // namespace UnoEngine
