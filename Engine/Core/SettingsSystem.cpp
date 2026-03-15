#include "pch.h"
#include "SettingsSystem.h"
#include "EventSystem.h"
#include "EngineEvents.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace UnoEngine {

SettingsSystem& SettingsSystem::GetInstance() {
    static SettingsSystem instance;
    return instance;
}

bool SettingsSystem::Load(const std::string& filepath) {
    try {
        if (!std::filesystem::exists(filepath)) {
            Logger::Info("Settings file not found: %s, using defaults", filepath.c_str());
            Save(filepath);
            return true;
        }

        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::Warning("Failed to open settings file: %s", filepath.c_str());
            return false;
        }

        json j = json::parse(file, nullptr, false);
        if (j.is_discarded()) {
            Logger::Warning("Failed to parse settings file: %s, using defaults", filepath.c_str());
            ResetToDefaults();
            return false;
        }

        FromJson(j);
        Logger::Info("Settings loaded from: %s", filepath.c_str());
        return true;
    }
    catch (const std::exception& e) {
        Logger::Warning("Exception loading settings: %s, using defaults", e.what());
        ResetToDefaults();
        return false;
    }
}

bool SettingsSystem::Save(const std::string& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::Warning("Failed to open settings file for writing: %s", filepath.c_str());
            return false;
        }

        file << ToJson().dump(4);
        Logger::Info("Settings saved to: %s", filepath.c_str());
        return true;
    }
    catch (const std::exception& e) {
        Logger::Warning("Exception saving settings: %s", e.what());
        return false;
    }
}

void SettingsSystem::ResetToDefaults() {
    graphics_ = GraphicsSettings{};
    audio_ = AudioSettings{};
    input_ = InputSettings{};
    game_ = GameSettings{};
}

void SettingsSystem::ApplyAll() {
    // 各サブシステムに設定を適用
    // 注: サブシステムがまだ初期化されていない場合もあるため安全にチェック

    // イベントで通知（各サブシステムが自分で対応する）
    NotifyChanged();
}

void SettingsSystem::NotifyChanged() {
    EventSystem::GetInstance().Fire<SettingsChangedEvent>();
}

nlohmann::json SettingsSystem::ToJson() const {
    json j;

    // グラフィックス設定
    j["graphics"] = {
        {"resolutionWidth", graphics_.resolutionWidth},
        {"resolutionHeight", graphics_.resolutionHeight},
        {"fullscreen", graphics_.fullscreen},
        {"vsync", graphics_.vsync},
        {"qualityLevel", graphics_.qualityLevel},
        {"renderScale", graphics_.renderScale}
    };

    // オーディオ設定
    j["audio"] = {
        {"masterVolume", audio_.masterVolume},
        {"musicVolume", audio_.musicVolume},
        {"sfxVolume", audio_.sfxVolume},
        {"muteAll", audio_.muteAll}
    };

    // 入力設定
    j["input"] = {
        {"mouseSensitivity", input_.mouseSensitivity},
        {"invertY", input_.invertY},
        {"gamepadSensitivity", input_.gamepadSensitivity},
        {"gamepadDeadzone", input_.gamepadDeadzone}
    };

    // ゲーム設定
    j["game"] = {
        {"language", game_.language},
        {"showFPS", game_.showFPS},
        {"fieldOfView", game_.fieldOfView}
    };

    return j;
}

void SettingsSystem::FromJson(const nlohmann::json& j) {
    // グラフィックス設定
    if (j.contains("graphics")) {
        const auto& g = j["graphics"];
        if (g.contains("resolutionWidth"))  graphics_.resolutionWidth  = g["resolutionWidth"].get<uint32_t>();
        if (g.contains("resolutionHeight")) graphics_.resolutionHeight = g["resolutionHeight"].get<uint32_t>();
        if (g.contains("fullscreen"))       graphics_.fullscreen       = g["fullscreen"].get<bool>();
        if (g.contains("vsync"))            graphics_.vsync            = g["vsync"].get<bool>();
        if (g.contains("qualityLevel"))     graphics_.qualityLevel     = g["qualityLevel"].get<int32_t>();
        if (g.contains("renderScale"))      graphics_.renderScale      = g["renderScale"].get<float>();
    }

    // オーディオ設定
    if (j.contains("audio")) {
        const auto& a = j["audio"];
        if (a.contains("masterVolume")) audio_.masterVolume = a["masterVolume"].get<float>();
        if (a.contains("musicVolume"))  audio_.musicVolume  = a["musicVolume"].get<float>();
        if (a.contains("sfxVolume"))    audio_.sfxVolume    = a["sfxVolume"].get<float>();
        if (a.contains("muteAll"))      audio_.muteAll      = a["muteAll"].get<bool>();
    }

    // 入力設定
    if (j.contains("input")) {
        const auto& i = j["input"];
        if (i.contains("mouseSensitivity"))   input_.mouseSensitivity   = i["mouseSensitivity"].get<float>();
        if (i.contains("invertY"))            input_.invertY            = i["invertY"].get<bool>();
        if (i.contains("gamepadSensitivity")) input_.gamepadSensitivity = i["gamepadSensitivity"].get<float>();
        if (i.contains("gamepadDeadzone"))    input_.gamepadDeadzone    = i["gamepadDeadzone"].get<float>();
    }

    // ゲーム設定
    if (j.contains("game")) {
        const auto& gm = j["game"];
        if (gm.contains("language"))    game_.language    = gm["language"].get<std::string>();
        if (gm.contains("showFPS"))     game_.showFPS     = gm["showFPS"].get<bool>();
        if (gm.contains("fieldOfView")) game_.fieldOfView = gm["fieldOfView"].get<float>();
    }
}

} // namespace UnoEngine
