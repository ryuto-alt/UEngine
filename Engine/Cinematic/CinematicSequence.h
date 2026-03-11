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
// CinematicSequence - カメラキーフレーム列
// ============================================================
struct CinematicSequence {
    std::string name     = "Intro";
    float       duration = 10.0f;   // 総再生時間（秒）
    bool        loop     = false;
    std::vector<CameraKeyframe> keyframes;

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
