#pragma once

#include "../Math/Math.h"
#include <vector>
#include <string>
#include <optional>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace UnoEngine {

// ============================================================
// CameraKeyframe - 1つのカメラポーズ
// ============================================================
struct CameraKeyframe {
    float     time     = 0.0f;       // 絶対時間（秒）
    Vector3   position;              // カメラ位置
    Quaternion rotation;             // カメラ回転
    float     fov      = 60.0f;      // Field of View（度）

    // 前のキーフレームからこのキーフレームへの補間方式
    enum class Easing { Linear, EaseIn, EaseOut, EaseInOut, Emphasis } easing = Easing::EaseInOut;

    CameraKeyframe() = default;
    CameraKeyframe(float t, const Vector3& pos, const Quaternion& rot, float fovDeg = 60.0f)
        : time(t), position(pos), rotation(rot), fov(fovDeg) {}
};

// ============================================================
// CinematicEvent - タイムライン上のイベント
// ============================================================
enum class CinematicEventType {
    Text,           // テキスト表示
    Audio,          // SE/BGM再生
    ObjectToggle,   // オブジェクト表示/非表示
    Lua,            // Luaスクリプト呼び出し
    WaitForInput    // 入力待ち（シネマティック一時停止）
};

enum class TextDisplayStyle {
    Subtitle,       // 画面下部字幕
    CenterDialog,   // 画面中央ダイアログ
    Bubble          // 対象付近の吹き出し
};

struct CinematicEvent {
    CinematicEventType type = CinematicEventType::Text;
    float time     = 0.0f;   // 開始時刻（秒）
    float duration = 3.0f;   // 表示/持続時間（秒）
    float fadeIn   = 0.3f;   // フェードイン時間
    float fadeOut  = 0.3f;   // フェードアウト時間

    // Text
    std::string text;
    TextDisplayStyle displayStyle = TextDisplayStyle::Subtitle;
    float fontSize = 1.0f;

    // Audio
    std::string audioClip;
    float volume = 1.0f;

    // ObjectToggle
    std::string targetObject;
    bool visible = true;

    // Lua
    std::string luaFunction;

    // WaitForInput
    std::string waitKey;      // 空 = 任意キー
};

// ============================================================
// CinematicSequence - カメラキーフレーム列 + イベントトラック
// ============================================================
struct CinematicSequence {
    std::string name     = "Intro";
    float       duration = 10.0f;   // 総再生時間（秒）
    bool        loop     = false;
    std::vector<CameraKeyframe> keyframes;
    std::vector<CinematicEvent> events;

    // JSON シリアライズ
    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["name"]     = name;
        j["duration"] = duration;
        j["loop"]     = loop;

        auto kfs = nlohmann::json::array();
        for (const auto& kf : keyframes) {
            nlohmann::json k;
            k["time"]   = kf.time;
            k["fov"]    = kf.fov;
            k["easing"] = static_cast<int>(kf.easing);
            k["position"] = { kf.position.GetX(), kf.position.GetY(), kf.position.GetZ() };
            k["rotation"] = { kf.rotation.GetX(), kf.rotation.GetY(),
                              kf.rotation.GetZ(), kf.rotation.GetW() };
            kfs.push_back(k);
        }
        j["keyframes"] = kfs;

        auto evts = nlohmann::json::array();
        for (const auto& ev : events) {
            nlohmann::json e;
            e["type"]         = static_cast<int>(ev.type);
            e["time"]         = ev.time;
            e["duration"]     = ev.duration;
            e["fadeIn"]       = ev.fadeIn;
            e["fadeOut"]      = ev.fadeOut;
            e["text"]         = ev.text;
            e["displayStyle"] = static_cast<int>(ev.displayStyle);
            e["fontSize"]     = ev.fontSize;
            e["audioClip"]    = ev.audioClip;
            e["volume"]       = ev.volume;
            e["targetObject"] = ev.targetObject;
            e["visible"]      = ev.visible;
            e["luaFunction"]  = ev.luaFunction;
            e["waitKey"]      = ev.waitKey;
            evts.push_back(e);
        }
        j["events"] = evts;

        return j;
    }

    // JSON デシリアライズ
    static CinematicSequence FromJson(const nlohmann::json& j) {
        CinematicSequence seq;
        seq.name     = j.value("name",     "Intro");
        seq.duration = j.value("duration", 10.0f);
        seq.loop     = j.value("loop",     false);

        if (j.contains("keyframes")) {
            for (const auto& k : j["keyframes"]) {
                CameraKeyframe kf;
                kf.time   = k.value("time",   0.0f);
                kf.fov    = k.value("fov",    60.0f);
                kf.easing = static_cast<CameraKeyframe::Easing>(k.value("easing", 3));

                if (k.contains("position")) {
                    const auto& p = k["position"];
                    kf.position = Vector3(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
                }
                if (k.contains("rotation")) {
                    const auto& r = k["rotation"];
                    kf.rotation = Quaternion(r[0].get<float>(), r[1].get<float>(),
                                             r[2].get<float>(), r[3].get<float>());
                }
                seq.keyframes.push_back(kf);
            }
        }

        if (j.contains("events")) {
            for (const auto& e : j["events"]) {
                CinematicEvent ev;
                ev.type         = static_cast<CinematicEventType>(e.value("type", 0));
                ev.time         = e.value("time", 0.0f);
                ev.duration     = e.value("duration", 3.0f);
                ev.fadeIn       = e.value("fadeIn", 0.3f);
                ev.fadeOut      = e.value("fadeOut", 0.3f);
                ev.text         = e.value("text", "");
                ev.displayStyle = static_cast<TextDisplayStyle>(e.value("displayStyle", 0));
                ev.fontSize     = e.value("fontSize", 1.0f);
                ev.audioClip    = e.value("audioClip", "");
                ev.volume       = e.value("volume", 1.0f);
                ev.targetObject = e.value("targetObject", "");
                ev.visible      = e.value("visible", true);
                ev.luaFunction  = e.value("luaFunction", "");
                ev.waitKey      = e.value("waitKey", "");
                seq.events.push_back(ev);
            }
        }

        return seq;
    }

    // ファイル保存
    bool SaveToFile(const std::string& filepath) const {
        try {
            // ディレクトリ作成
            std::filesystem::path dir = std::filesystem::path(filepath).parent_path();
            if (!dir.empty()) std::filesystem::create_directories(dir);

            std::ofstream file(filepath);
            if (!file.is_open()) return false;
            file << ToJson().dump(4);
            return true;
        } catch (...) { return false; }
    }

    // ファイル読み込み
    static std::optional<CinematicSequence> LoadFromFile(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) return std::nullopt;
            nlohmann::json j = nlohmann::json::parse(file);
            return FromJson(j);
        } catch (...) { return std::nullopt; }
    }
};

} // namespace UnoEngine
