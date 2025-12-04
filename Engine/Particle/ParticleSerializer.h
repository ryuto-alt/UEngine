#pragma once

#include "../Core/Types.h"
#include "ParticleData.h"
#include "ParticleEmitter.h"
#include "Curve.h"
#include "Gradient.h"
#include <string>
#include <vector>

namespace UnoEngine {

class ParticleSystem;

// パーティクルエフェクト全体の定義
struct ParticleEffectData {
    std::string name;
    std::string version = "1.0";
    std::vector<EmitterConfig> emitters;
};

// JSON シリアライザ
class ParticleSerializer {
public:
    // エフェクト全体の保存/読み込み
    static bool SaveEffect(const std::string& path, const ParticleEffectData& effect);
    static bool LoadEffect(const std::string& path, ParticleEffectData& effect);

    // ParticleSystemから直接保存/読み込み
    static bool SaveParticleSystem(const std::string& path, ParticleSystem* system);
    static bool LoadParticleSystem(const std::string& path, ParticleSystem* system);

    // 単一エミッター設定の保存/読み込み
    static bool SaveEmitterConfig(const std::string& path, const EmitterConfig& config);
    static bool LoadEmitterConfig(const std::string& path, EmitterConfig& config);

    // JSON文字列への変換
    static std::string ToJson(const ParticleEffectData& effect);
    static std::string ToJson(const EmitterConfig& config);

    // JSON文字列からの読み込み
    static bool FromJson(const std::string& json, ParticleEffectData& effect);
    static bool FromJson(const std::string& json, EmitterConfig& config);

private:
    // 内部ヘルパー
    static std::string SerializeCurve(const AnimationCurve& curve);
    static std::string SerializeMinMaxCurve(const MinMaxCurve& curve);
    static std::string SerializeGradient(const Gradient& gradient);
    static std::string SerializeMinMaxGradient(const MinMaxGradient& gradient);
    static std::string SerializeFloat3(const Float3& v);
    static std::string SerializeFloat4(const Float4& v);

    static bool DeserializeCurve(const std::string& json, AnimationCurve& curve);
    static bool DeserializeMinMaxCurve(const std::string& json, MinMaxCurve& curve);
    static bool DeserializeGradient(const std::string& json, Gradient& gradient);
    static bool DeserializeMinMaxGradient(const std::string& json, MinMaxGradient& gradient);
};

} // namespace UnoEngine
