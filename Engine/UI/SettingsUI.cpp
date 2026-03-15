#include "pch.h"
#include "SettingsUI.h"
#include "../Core/SettingsSystem.h"
#include <imgui.h>

namespace UnoEngine {

void SettingsUI::Render() {
    if (!isOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings", &isOpen_)) {
        ImGui::End();
        return;
    }

    auto& settings = SettingsSystem::GetInstance();

    if (ImGui::BeginTabBar("SettingsTabs")) {
        if (ImGui::BeginTabItem("Graphics")) {
            RenderGraphicsTab(settings.GetGraphics());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Audio")) {
            RenderAudioTab(settings.GetAudio());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Input")) {
            RenderInputTab(settings.GetInput());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Game")) {
            RenderGameTab(settings.GetGame());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // 適用・リセットボタン
    if (ImGui::Button("Apply")) {
        settings.ApplyAll();
        settings.Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        settings.ResetToDefaults();
        settings.ApplyAll();
        settings.Save();
    }

    ImGui::End();
}

void SettingsUI::RenderGraphicsTab(GraphicsSettings& settings) {
    // 解像度選択
    struct Resolution {
        uint32_t width;
        uint32_t height;
        const char* label;
    };
    static const Resolution resolutions[] = {
        {1280,  720, "1280 x 720 (720p)"},
        {1600,  900, "1600 x 900"},
        {1920, 1080, "1920 x 1080 (1080p)"},
        {2560, 1440, "2560 x 1440 (1440p)"},
        {3840, 2160, "3840 x 2160 (4K)"}
    };

    // 現在の解像度に対応するインデックスを検索
    int currentIndex = 0;
    for (int i = 0; i < IM_ARRAYSIZE(resolutions); ++i) {
        if (resolutions[i].width == settings.resolutionWidth &&
            resolutions[i].height == settings.resolutionHeight) {
            currentIndex = i;
            break;
        }
    }

    if (ImGui::Combo("Resolution", &currentIndex, [](void* data, int idx) -> const char* {
        return static_cast<const Resolution*>(data)[idx].label;
    }, (void*)resolutions, IM_ARRAYSIZE(resolutions))) {
        settings.resolutionWidth = resolutions[currentIndex].width;
        settings.resolutionHeight = resolutions[currentIndex].height;
    }

    ImGui::Checkbox("Fullscreen", &settings.fullscreen);
    ImGui::Checkbox("VSync", &settings.vsync);

    // 品質レベル
    const char* qualityLabels[] = {"Low", "Medium", "High"};
    ImGui::SliderInt("Quality", &settings.qualityLevel, 0, 2, qualityLabels[settings.qualityLevel]);

    ImGui::SliderFloat("Render Scale", &settings.renderScale, 0.5f, 2.0f, "%.1f");
}

void SettingsUI::RenderAudioTab(AudioSettings& settings) {
    ImGui::SliderFloat("Master Volume", &settings.masterVolume, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Music Volume", &settings.musicVolume, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("SFX Volume", &settings.sfxVolume, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Mute All", &settings.muteAll);
}

void SettingsUI::RenderInputTab(InputSettings& settings) {
    ImGui::SliderFloat("Mouse Sensitivity", &settings.mouseSensitivity, 0.1f, 5.0f, "%.2f");
    ImGui::Checkbox("Invert Y Axis", &settings.invertY);
    ImGui::SliderFloat("Gamepad Sensitivity", &settings.gamepadSensitivity, 0.1f, 5.0f, "%.2f");
    ImGui::SliderFloat("Gamepad Deadzone", &settings.gamepadDeadzone, 0.05f, 0.5f, "%.2f");
}

void SettingsUI::RenderGameTab(GameSettings& settings) {
    // 言語選択
    const char* languages[] = {"ja", "en"};
    const char* languageLabels[] = {"Japanese", "English"};
    int currentLang = 0;
    for (int i = 0; i < IM_ARRAYSIZE(languages); ++i) {
        if (settings.language == languages[i]) {
            currentLang = i;
            break;
        }
    }
    if (ImGui::Combo("Language", &currentLang, languageLabels, IM_ARRAYSIZE(languageLabels))) {
        settings.language = languages[currentLang];
    }

    ImGui::Checkbox("Show FPS", &settings.showFPS);
    ImGui::SliderFloat("Field of View", &settings.fieldOfView, 50.0f, 120.0f, "%.0f");
}

} // namespace UnoEngine
