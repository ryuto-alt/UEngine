#include "pch.h"
#include "ParticleEmitter.h"
#include <cmath>

namespace UnoEngine {

uint32 ParticleEmitter::nextEmitterID_ = 0;

ParticleEmitter::ParticleEmitter()
    : emitterID_(nextEmitterID_++)
    , rng_(std::random_device{}())
{
}

ParticleEmitter::ParticleEmitter(const EmitterConfig& config)
    : config_(config)
    , emitterID_(nextEmitterID_++)
    , rng_(std::random_device{}())
{
    // バースト管理の初期化
    burstCycleCount_.resize(config_.bursts.size(), 0);
    burstNextTime_.resize(config_.bursts.size());
    for (size_t i = 0; i < config_.bursts.size(); ++i) {
        burstNextTime_[i] = config_.bursts[i].time;
    }
}

void ParticleEmitter::Update(float deltaTime) {
    if (!isPlaying_ || isPaused_) return;

    time_ += deltaTime;

    // バースト配列のサイズを同期（エディタでburstsが変更された場合に対応）
    if (burstCycleCount_.size() != config_.bursts.size()) {
        burstCycleCount_.resize(config_.bursts.size(), 0);
        burstNextTime_.resize(config_.bursts.size());
        for (size_t i = 0; i < config_.bursts.size(); ++i) {
            burstNextTime_[i] = config_.bursts[i].time;
        }
    }

    // ループ処理
    if (config_.looping && time_ >= config_.duration) {
        time_ = std::fmod(time_, config_.duration);
        // バーストリセット
        for (size_t i = 0; i < config_.bursts.size(); ++i) {
            if (config_.bursts[i].cycles == 0 || burstCycleCount_[i] < config_.bursts[i].cycles) {
                burstNextTime_[i] = config_.bursts[i].time;
            }
        }
    }

    // 非ループで終了
    if (!config_.looping && time_ >= config_.duration) {
        isPlaying_ = false;
    }
}

void ParticleEmitter::Play() {
    isPlaying_ = true;
    isPaused_ = false;
    if (time_ >= config_.duration && !config_.looping) {
        Restart();
    }
}

void ParticleEmitter::Pause() {
    isPaused_ = true;
}

void ParticleEmitter::Stop() {
    isPlaying_ = false;
    isPaused_ = false;
}

void ParticleEmitter::Restart() {
    time_ = 0.0f;
    emitAccumulator_ = 0.0f;
    isPlaying_ = true;
    isPaused_ = false;

    // バーストリセット
    burstCycleCount_.assign(config_.bursts.size(), 0);
    burstNextTime_.resize(config_.bursts.size());
    for (size_t i = 0; i < config_.bursts.size(); ++i) {
        burstNextTime_[i] = config_.bursts[i].time;
    }
}

uint32 ParticleEmitter::CalculateEmitCount(float deltaTime) {
    if (!isPlaying_ || isPaused_) return 0;
    if (time_ < config_.startDelay) return 0;

    uint32 emitCount = 0;

    // Rate over time
    if (config_.emitRate > 0.0f) {
        emitAccumulator_ += config_.emitRate * deltaTime;
        uint32 rateEmit = static_cast<uint32>(emitAccumulator_);
        emitAccumulator_ -= rateEmit;
        emitCount += rateEmit;
    }

    // Bursts
    for (size_t i = 0; i < config_.bursts.size(); ++i) {
        auto& burst = config_.bursts[i];

        // サイクル制限チェック
        if (burst.cycles > 0 && burstCycleCount_[i] >= burst.cycles) {
            continue;
        }

        // 発生時刻チェック
        if (time_ >= burstNextTime_[i]) {
            // 確率チェック
            if (uniformDist_(rng_) <= burst.probability) {
                emitCount += burst.count;
            }

            burstCycleCount_[i]++;

            // 次の発生時刻
            if (burst.interval > 0.0f) {
                burstNextTime_[i] += burst.interval;
            } else {
                burstNextTime_[i] = config_.duration + 1.0f;  // 無効化
            }
        }
    }

    // 最大パーティクル数制限
    emitCount = std::min(emitCount, config_.maxParticles);

    return emitCount;
}

float ParticleEmitter::SampleLifetime() const {
    float random = uniformDist_(rng_);
    return config_.startLifetime.Evaluate(0.0f, random);
}

float ParticleEmitter::SampleSpeed() const {
    float random = uniformDist_(rng_);
    return config_.startSpeed.Evaluate(0.0f, random);
}

float ParticleEmitter::SampleSize() const {
    float random = uniformDist_(rng_);
    return config_.startSize.Evaluate(0.0f, random);
}

Float4 ParticleEmitter::SampleColor() const {
    float random = uniformDist_(rng_);
    return config_.startColor.Evaluate(0.0f, random);
}

float ParticleEmitter::SampleRotation() const {
    float random = uniformDist_(rng_);
    // 度からラジアンに変換
    return config_.startRotation.Evaluate(0.0f, random) * 3.14159265f / 180.0f;
}

void ParticleEmitter::SampleShape(Float3& outPosition, Float3& outDirection) const {
    const auto& shape = config_.shape;

    auto randomFloat = [this]() { return uniformDist_(rng_); };
    auto randomRange = [&](float min, float max) { return min + randomFloat() * (max - min); };

    switch (shape.shape) {
    case EmitShape::Point:
        outPosition = { 0.0f, 0.0f, 0.0f };
        // ランダム方向
        {
            float theta = randomFloat() * 6.28318530718f;
            float phi = std::acos(2.0f * randomFloat() - 1.0f);
            float sinPhi = std::sin(phi);
            outDirection = {
                sinPhi * std::cos(theta),
                sinPhi * std::sin(theta),
                std::cos(phi)
            };
        }
        break;

    case EmitShape::Sphere:
        {
            float theta = randomFloat() * 6.28318530718f;
            float phi = std::acos(2.0f * randomFloat() - 1.0f);
            float sinPhi = std::sin(phi);
            Float3 dir = {
                sinPhi * std::cos(theta),
                sinPhi * std::sin(theta),
                std::cos(phi)
            };
            float r = shape.emitFromEdge ? shape.radius :
                      std::pow(randomFloat(), 1.0f / 3.0f) * shape.radius;
            outPosition = { dir.x * r, dir.y * r, dir.z * r };
            outDirection = shape.randomDirection ? Float3{
                sinPhi * std::cos(randomFloat() * 6.28318530718f),
                sinPhi * std::sin(randomFloat() * 6.28318530718f),
                std::cos(std::acos(2.0f * randomFloat() - 1.0f))
            } : dir;
        }
        break;

    case EmitShape::Hemisphere:
        {
            float theta = randomFloat() * 6.28318530718f;
            float phi = std::acos(randomFloat());  // 0 to pi/2
            float sinPhi = std::sin(phi);
            Float3 dir = {
                sinPhi * std::cos(theta),
                std::cos(phi),  // Y-up
                sinPhi * std::sin(theta)
            };
            float r = shape.emitFromEdge ? shape.radius :
                      std::pow(randomFloat(), 1.0f / 3.0f) * shape.radius;
            outPosition = { dir.x * r, dir.y * r, dir.z * r };
            outDirection = dir;
        }
        break;

    case EmitShape::Box:
        outPosition = {
            randomRange(-shape.boxSize.x / 2, shape.boxSize.x / 2),
            randomRange(-shape.boxSize.y / 2, shape.boxSize.y / 2),
            randomRange(-shape.boxSize.z / 2, shape.boxSize.z / 2)
        };
        outDirection = { 0.0f, 1.0f, 0.0f };  // デフォルトで上向き
        break;

    case EmitShape::Cone:
        {
            float angle = randomFloat() * shape.arcAngle * 3.14159265f / 180.0f;
            float coneAngleRad = shape.coneAngle * 3.14159265f / 180.0f;
            float radius = randomFloat() * shape.coneRadius;

            outPosition = {
                std::cos(angle) * radius,
                0.0f,
                std::sin(angle) * radius
            };

            // 円錐方向
            float spreadAngle = randomFloat() * coneAngleRad;
            float spreadDir = randomFloat() * 6.28318530718f;
            float sinSpread = std::sin(spreadAngle);
            outDirection = {
                sinSpread * std::cos(spreadDir),
                std::cos(spreadAngle),
                sinSpread * std::sin(spreadDir)
            };
        }
        break;

    case EmitShape::Circle:
        {
            float angle = randomFloat() * shape.arcAngle * 3.14159265f / 180.0f;
            float r = shape.emitFromEdge ? shape.radius :
                      std::sqrt(randomFloat()) * shape.radius;
            outPosition = {
                std::cos(angle) * r,
                0.0f,
                std::sin(angle) * r
            };
            outDirection = { 0.0f, 1.0f, 0.0f };
        }
        break;

    case EmitShape::Edge:
        {
            float t = randomFloat();
            outPosition = {
                randomRange(-shape.radius, shape.radius),
                0.0f,
                0.0f
            };
            outDirection = { 0.0f, 1.0f, 0.0f };
        }
        break;

    default:
        outPosition = { 0.0f, 0.0f, 0.0f };
        outDirection = { 0.0f, 1.0f, 0.0f };
        break;
    }

    // ローカルオフセット適用
    outPosition.x += shape.position.x;
    outPosition.y += shape.position.y;
    outPosition.z += shape.position.z;
}

} // namespace UnoEngine
