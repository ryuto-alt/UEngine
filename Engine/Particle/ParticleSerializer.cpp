#include "pch.h"
#include "ParticleSerializer.h"
#include "ParticleSystem.h"
#include "../Core/Logger.h"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace UnoEngine {

// Float3/Float4のJSON変換
void to_json(json& j, const Float3& v) {
    j = json::array({ v.x, v.y, v.z });
}

void from_json(const json& j, Float3& v) {
    v.x = j[0].get<float>();
    v.y = j[1].get<float>();
    v.z = j[2].get<float>();
}

void to_json(json& j, const Float4& v) {
    j = json::array({ v.x, v.y, v.z, v.w });
}

void from_json(const json& j, Float4& v) {
    v.x = j[0].get<float>();
    v.y = j[1].get<float>();
    v.z = j[2].get<float>();
    v.w = j[3].get<float>();
}

// CurveKeyframeのJSON変換
void to_json(json& j, const CurveKeyframe& k) {
    j = json{
        {"time", k.time},
        {"value", k.value},
        {"inTangent", k.inTangent},
        {"outTangent", k.outTangent}
    };
}

void from_json(const json& j, CurveKeyframe& k) {
    k.time = j.value("time", 0.0f);
    k.value = j.value("value", 0.0f);
    k.inTangent = j.value("inTangent", 0.0f);
    k.outTangent = j.value("outTangent", 0.0f);
}

// AnimationCurveのJSON変換
void to_json(json& j, const AnimationCurve& curve) {
    j = json{
        {"interpolation", static_cast<int>(curve.GetInterpolation())},
        {"keys", curve.GetKeys()}
    };
}

void from_json(const json& j, AnimationCurve& curve) {
    curve.GetKeys().clear();
    if (j.contains("keys")) {
        for (const auto& keyJson : j["keys"]) {
            CurveKeyframe k;
            from_json(keyJson, k);
            curve.AddKey(k);
        }
    }
    if (j.contains("interpolation")) {
        curve.SetInterpolation(static_cast<CurveInterpolation>(j["interpolation"].get<int>()));
    }
}

// MinMaxCurveのJSON変換
void to_json(json& j, const MinMaxCurve& curve) {
    j = json{
        {"mode", static_cast<int>(curve.mode)},
        {"constantMin", curve.constantMin},
        {"constantMax", curve.constantMax},
        {"curveMin", curve.curveMin},
        {"curveMax", curve.curveMax},
        {"multiplier", curve.curveMultiplier}
    };
}

void from_json(const json& j, MinMaxCurve& curve) {
    curve.mode = static_cast<MinMaxCurveMode>(j.value("mode", 0));
    curve.constantMin = j.value("constantMin", 0.0f);
    curve.constantMax = j.value("constantMax", 1.0f);
    curve.curveMultiplier = j.value("multiplier", 1.0f);
    if (j.contains("curveMin")) from_json(j["curveMin"], curve.curveMin);
    if (j.contains("curveMax")) from_json(j["curveMax"], curve.curveMax);
}

// GradientColorKeyのJSON変換
void to_json(json& j, const GradientColorKey& k) {
    j = json{{"color", k.color}, {"time", k.time}};
}

void from_json(const json& j, GradientColorKey& k) {
    from_json(j["color"], k.color);
    k.time = j.value("time", 0.0f);
}

// GradientAlphaKeyのJSON変換
void to_json(json& j, const GradientAlphaKey& k) {
    j = json{{"alpha", k.alpha}, {"time", k.time}};
}

void from_json(const json& j, GradientAlphaKey& k) {
    k.alpha = j.value("alpha", 1.0f);
    k.time = j.value("time", 0.0f);
}

// GradientのJSON変換
void to_json(json& j, const Gradient& gradient) {
    j = json{
        {"colorKeys", gradient.GetColorKeys()},
        {"alphaKeys", gradient.GetAlphaKeys()}
    };
}

void from_json(const json& j, Gradient& gradient) {
    gradient.GetColorKeys().clear();
    gradient.GetAlphaKeys().clear();
    if (j.contains("colorKeys")) {
        for (const auto& keyJson : j["colorKeys"]) {
            GradientColorKey k;
            from_json(keyJson, k);
            gradient.AddColorKey(k.color, k.time);
        }
    }
    if (j.contains("alphaKeys")) {
        for (const auto& keyJson : j["alphaKeys"]) {
            GradientAlphaKey k;
            from_json(keyJson, k);
            gradient.AddAlphaKey(k.alpha, k.time);
        }
    }
}

// MinMaxGradientのJSON変換
void to_json(json& j, const MinMaxGradient& gradient) {
    json gradMinJson, gradMaxJson;
    to_json(gradMinJson, gradient.gradientMin);
    to_json(gradMaxJson, gradient.gradientMax);

    j = json{
        {"mode", static_cast<int>(gradient.mode)},
        {"colorMin", gradient.colorMin},
        {"colorMax", gradient.colorMax},
        {"gradientMin", gradMinJson},
        {"gradientMax", gradMaxJson}
    };
}

void from_json(const json& j, MinMaxGradient& gradient) {
    gradient.mode = static_cast<MinMaxGradientMode>(j.value("mode", 0));
    if (j.contains("colorMin")) from_json(j["colorMin"], gradient.colorMin);
    if (j.contains("colorMax")) from_json(j["colorMax"], gradient.colorMax);
    if (j.contains("gradientMin")) from_json(j["gradientMin"], gradient.gradientMin);
    if (j.contains("gradientMax")) from_json(j["gradientMax"], gradient.gradientMax);
}

// BurstConfigのJSON変換
void to_json(json& j, const BurstConfig& burst) {
    j = json{
        {"time", burst.time},
        {"count", burst.count},
        {"cycles", burst.cycles},
        {"interval", burst.interval},
        {"probability", burst.probability}
    };
}

void from_json(const json& j, BurstConfig& burst) {
    burst.time = j.value("time", 0.0f);
    burst.count = j.value("count", 10);
    burst.cycles = j.value("cycles", 1);
    burst.interval = j.value("interval", 0.0f);
    burst.probability = j.value("probability", 1.0f);
}

// ShapeConfigのJSON変換
void to_json(json& j, const ShapeConfig& shape) {
    j = json{
        {"shape", static_cast<int>(shape.shape)},
        {"radius", shape.radius},
        {"boxSize", shape.boxSize},
        {"coneAngle", shape.coneAngle},
        {"coneRadius", shape.coneRadius},
        {"arcAngle", shape.arcAngle},
        {"position", shape.position},
        {"rotation", shape.rotation},
        {"emitFromEdge", shape.emitFromEdge},
        {"randomDirection", shape.randomDirection}
    };
}

void from_json(const json& j, ShapeConfig& shape) {
    shape.shape = static_cast<EmitShape>(j.value("shape", 0));
    shape.radius = j.value("radius", 1.0f);
    if (j.contains("boxSize")) from_json(j["boxSize"], shape.boxSize);
    shape.coneAngle = j.value("coneAngle", 25.0f);
    shape.coneRadius = j.value("coneRadius", 1.0f);
    shape.arcAngle = j.value("arcAngle", 360.0f);
    if (j.contains("position")) from_json(j["position"], shape.position);
    if (j.contains("rotation")) from_json(j["rotation"], shape.rotation);
    shape.emitFromEdge = j.value("emitFromEdge", false);
    shape.randomDirection = j.value("randomDirection", false);
}

// CollisionConfigのJSON変換
void to_json(json& j, const CollisionConfig& collision) {
    j = json{
        {"enabled", collision.enabled},
        {"bounce", collision.bounce},
        {"lifetimeLoss", collision.lifetimeLoss},
        {"minKillSpeed", collision.minKillSpeed},
        {"killOnCollision", collision.killOnCollision},
        {"radiusScale", collision.radiusScale}
    };
}

void from_json(const json& j, CollisionConfig& collision) {
    collision.enabled = j.value("enabled", false);
    collision.bounce = j.value("bounce", 0.5f);
    collision.lifetimeLoss = j.value("lifetimeLoss", 0.0f);
    collision.minKillSpeed = j.value("minKillSpeed", 0.0f);
    collision.killOnCollision = j.value("killOnCollision", false);
    collision.radiusScale = j.value("radiusScale", 1.0f);
}

// SubEmitterConfigのJSON変換
void to_json(json& j, const SubEmitterConfig& sub) {
    j = json{
        {"trigger", static_cast<int>(sub.trigger)},
        {"emitterName", sub.emitterName},
        {"emitCount", sub.emitCount},
        {"probability", sub.probability}
    };
}

void from_json(const json& j, SubEmitterConfig& sub) {
    sub.trigger = static_cast<SubEmitterConfig::Trigger>(j.value("trigger", 1));
    sub.emitterName = j.value("emitterName", "");
    sub.emitCount = j.value("emitCount", 1);
    sub.probability = j.value("probability", 1.0f);
}

// SpriteSheetConfigのJSON変換
void to_json(json& j, const SpriteSheetConfig& sheet) {
    j = json{
        {"enabled", sheet.enabled},
        {"tilesX", sheet.tilesX},
        {"tilesY", sheet.tilesY},
        {"frameCount", sheet.frameCount},
        {"fps", sheet.fps},
        {"startFrame", sheet.startFrame},
        {"loop", sheet.loop}
    };
}

void from_json(const json& j, SpriteSheetConfig& sheet) {
    sheet.enabled = j.value("enabled", false);
    sheet.tilesX = j.value("tilesX", 1);
    sheet.tilesY = j.value("tilesY", 1);
    sheet.frameCount = j.value("frameCount", 1);
    sheet.fps = j.value("fps", 30.0f);
    sheet.startFrame = j.value("startFrame", 0);
    sheet.loop = j.value("loop", true);
}

// VelocityOverLifetimeのJSON変換
void to_json(json& j, const VelocityOverLifetime& vel) {
    j = json{
        {"enabled", vel.enabled},
        {"speedMultiplier", vel.speedMultiplier},
        {"x", vel.x},
        {"y", vel.y},
        {"z", vel.z},
        {"isLocal", vel.isLocal}
    };
}

void from_json(const json& j, VelocityOverLifetime& vel) {
    vel.enabled = j.value("enabled", false);
    if (j.contains("speedMultiplier")) from_json(j["speedMultiplier"], vel.speedMultiplier);
    if (j.contains("x")) from_json(j["x"], vel.x);
    if (j.contains("y")) from_json(j["y"], vel.y);
    if (j.contains("z")) from_json(j["z"], vel.z);
    vel.isLocal = j.value("isLocal", false);
}

// ColorOverLifetimeのJSON変換
void to_json(json& j, const ColorOverLifetime& col) {
    j = json{
        {"enabled", col.enabled},
        {"color", col.color}
    };
}

void from_json(const json& j, ColorOverLifetime& col) {
    col.enabled = j.value("enabled", false);
    if (j.contains("color")) from_json(j["color"], col.color);
}

// SizeOverLifetimeのJSON変換
void to_json(json& j, const SizeOverLifetime& size) {
    j = json{
        {"enabled", size.enabled},
        {"size", size.size},
        {"separateAxes", size.separateAxes},
        {"x", size.x},
        {"y", size.y}
    };
}

void from_json(const json& j, SizeOverLifetime& size) {
    size.enabled = j.value("enabled", false);
    if (j.contains("size")) from_json(j["size"], size.size);
    size.separateAxes = j.value("separateAxes", false);
    if (j.contains("x")) from_json(j["x"], size.x);
    if (j.contains("y")) from_json(j["y"], size.y);
}

// RotationOverLifetimeのJSON変換
void to_json(json& j, const RotationOverLifetime& rot) {
    j = json{
        {"enabled", rot.enabled},
        {"angularVelocity", rot.angularVelocity}
    };
}

void from_json(const json& j, RotationOverLifetime& rot) {
    rot.enabled = j.value("enabled", false);
    if (j.contains("angularVelocity")) from_json(j["angularVelocity"], rot.angularVelocity);
}

// ForceOverLifetimeのJSON変換
void to_json(json& j, const ForceOverLifetime& force) {
    j = json{
        {"enabled", force.enabled},
        {"x", force.x},
        {"y", force.y},
        {"z", force.z},
        {"isLocal", force.isLocal}
    };
}

void from_json(const json& j, ForceOverLifetime& force) {
    force.enabled = j.value("enabled", false);
    if (j.contains("x")) from_json(j["x"], force.x);
    if (j.contains("y")) from_json(j["y"], force.y);
    if (j.contains("z")) from_json(j["z"], force.z);
    force.isLocal = j.value("isLocal", false);
}

// EmitterConfigのJSON変換
void to_json(json& j, const EmitterConfig& config) {
    j = json{
        {"name", config.name},
        {"duration", config.duration},
        {"looping", config.looping},
        {"prewarm", config.prewarm},
        {"startDelay", config.startDelay},
        {"maxParticles", config.maxParticles},
        {"emitRate", config.emitRate},
        {"bursts", config.bursts},
        {"shape", config.shape},
        {"startLifetime", config.startLifetime},
        {"startSpeed", config.startSpeed},
        {"startSize", config.startSize},
        {"startColor", config.startColor},
        {"startRotation", config.startRotation},
        {"velocityOverLifetime", config.velocityOverLifetime},
        {"colorOverLifetime", config.colorOverLifetime},
        {"sizeOverLifetime", config.sizeOverLifetime},
        {"rotationOverLifetime", config.rotationOverLifetime},
        {"forceOverLifetime", config.forceOverLifetime},
        {"renderMode", static_cast<int>(config.renderMode)},
        {"blendMode", static_cast<int>(config.blendMode)},
        {"texturePath", config.texturePath},
        {"spriteSheet", config.spriteSheet},
        {"collision", config.collision},
        {"subEmitters", config.subEmitters}
    };
}

void from_json(const json& j, EmitterConfig& config) {
    config.name = j.value("name", "Emitter");
    config.duration = j.value("duration", 5.0f);
    config.looping = j.value("looping", true);
    config.prewarm = j.value("prewarm", false);
    config.startDelay = j.value("startDelay", 0.0f);
    config.maxParticles = j.value("maxParticles", 1000u);
    config.emitRate = j.value("emitRate", 10.0f);

    if (j.contains("bursts")) {
        config.bursts.clear();
        for (const auto& burstJson : j["bursts"]) {
            BurstConfig burst;
            from_json(burstJson, burst);
            config.bursts.push_back(burst);
        }
    }

    if (j.contains("shape")) from_json(j["shape"], config.shape);
    if (j.contains("startLifetime")) from_json(j["startLifetime"], config.startLifetime);
    if (j.contains("startSpeed")) from_json(j["startSpeed"], config.startSpeed);
    if (j.contains("startSize")) from_json(j["startSize"], config.startSize);
    if (j.contains("startColor")) from_json(j["startColor"], config.startColor);
    if (j.contains("startRotation")) from_json(j["startRotation"], config.startRotation);
    if (j.contains("velocityOverLifetime")) from_json(j["velocityOverLifetime"], config.velocityOverLifetime);
    if (j.contains("colorOverLifetime")) from_json(j["colorOverLifetime"], config.colorOverLifetime);
    if (j.contains("sizeOverLifetime")) from_json(j["sizeOverLifetime"], config.sizeOverLifetime);
    if (j.contains("rotationOverLifetime")) from_json(j["rotationOverLifetime"], config.rotationOverLifetime);
    if (j.contains("forceOverLifetime")) from_json(j["forceOverLifetime"], config.forceOverLifetime);

    config.renderMode = static_cast<RenderMode>(j.value("renderMode", 0));
    config.blendMode = static_cast<BlendMode>(j.value("blendMode", 0));
    config.texturePath = j.value("texturePath", "");

    if (j.contains("spriteSheet")) from_json(j["spriteSheet"], config.spriteSheet);
    if (j.contains("collision")) from_json(j["collision"], config.collision);

    if (j.contains("subEmitters")) {
        config.subEmitters.clear();
        for (const auto& subJson : j["subEmitters"]) {
            SubEmitterConfig sub;
            from_json(subJson, sub);
            config.subEmitters.push_back(sub);
        }
    }
}

// ParticleEffectDataのJSON変換
void to_json(json& j, const ParticleEffectData& effect) {
    j = json{
        {"name", effect.name},
        {"version", effect.version},
        {"emitters", effect.emitters}
    };
}

void from_json(const json& j, ParticleEffectData& effect) {
    effect.name = j.value("name", "Unnamed Effect");
    effect.version = j.value("version", "1.0");
    effect.emitters.clear();
    if (j.contains("emitters")) {
        for (const auto& emitterJson : j["emitters"]) {
            EmitterConfig config;
            from_json(emitterJson, config);
            effect.emitters.push_back(config);
        }
    }
}

// パブリックAPI実装

bool ParticleSerializer::SaveEffect(const std::string& path, const ParticleEffectData& effect) {
    try {
        json j = effect;
        std::ofstream file(path);
        if (!file.is_open()) {
            Logger::Error("[ParticleSerializer] Failed to open file for writing: %s", path.c_str());
            return false;
        }
        file << j.dump(2);  // インデント2スペース
        Logger::Info("[ParticleSerializer] Saved effect to: %s", path.c_str());
        return true;
    } catch (const std::exception& e) {
        Logger::Error("[ParticleSerializer] Error saving effect: %s", e.what());
        return false;
    }
}

bool ParticleSerializer::LoadEffect(const std::string& path, ParticleEffectData& effect) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            Logger::Error("[ParticleSerializer] Failed to open file: %s", path.c_str());
            return false;
        }

        json j;
        file >> j;
        effect = j.get<ParticleEffectData>();

        Logger::Info("[ParticleSerializer] Loaded effect from: %s", path.c_str());
        return true;
    } catch (const std::exception& e) {
        Logger::Error("[ParticleSerializer] Error loading effect: %s", e.what());
        return false;
    }
}

bool ParticleSerializer::SaveParticleSystem(const std::string& path, ParticleSystem* system) {
    if (!system) return false;

    ParticleEffectData effect;
    effect.name = "Particle Effect";

    for (size_t i = 0; i < system->GetEmitterCount(); ++i) {
        auto* emitter = system->GetEmitter(i);
        if (emitter) {
            effect.emitters.push_back(emitter->GetConfig());
        }
    }

    return SaveEffect(path, effect);
}

bool ParticleSerializer::LoadParticleSystem(const std::string& path, ParticleSystem* system) {
    if (!system) return false;

    ParticleEffectData effect;
    if (!LoadEffect(path, effect)) {
        return false;
    }

    system->RemoveAllEmitters();
    for (const auto& config : effect.emitters) {
        auto* emitter = system->CreateEmitter(config);
        emitter->Play();
    }

    return true;
}

bool ParticleSerializer::SaveEmitterConfig(const std::string& path, const EmitterConfig& config) {
    try {
        json j = config;
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << j.dump(2);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ParticleSerializer::LoadEmitterConfig(const std::string& path, EmitterConfig& config) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        json j;
        file >> j;
        config = j.get<EmitterConfig>();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string ParticleSerializer::ToJson(const ParticleEffectData& effect) {
    json j = effect;
    return j.dump(2);
}

std::string ParticleSerializer::ToJson(const EmitterConfig& config) {
    json j = config;
    return j.dump(2);
}

bool ParticleSerializer::FromJson(const std::string& jsonStr, ParticleEffectData& effect) {
    try {
        json j = json::parse(jsonStr);
        effect = j.get<ParticleEffectData>();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ParticleSerializer::FromJson(const std::string& jsonStr, EmitterConfig& config) {
    try {
        json j = json::parse(jsonStr);
        config = j.get<EmitterConfig>();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace UnoEngine
