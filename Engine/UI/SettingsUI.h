#pragma once

#include "../Core/SettingsSystem.h"

namespace UnoEngine {

// 設定UIウィンドウ（ImGuiベース）
class SettingsUI {
public:
    SettingsUI() = default;
    ~SettingsUI() = default;

    // ImGuiフレーム内で呼び出す
    void Render();

    // 開閉状態
    bool IsOpen() const { return isOpen_; }
    void SetOpen(bool open) { isOpen_ = open; }
    void Toggle() { isOpen_ = !isOpen_; }

private:
    void RenderGraphicsTab(GraphicsSettings& settings);
    void RenderAudioTab(AudioSettings& settings);
    void RenderInputTab(InputSettings& settings);
    void RenderGameTab(GameSettings& settings);

    bool isOpen_ = false;
};

} // namespace UnoEngine
