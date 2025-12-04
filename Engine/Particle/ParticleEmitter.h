#pragma once

#include "../Core/Types.h"
#include "../Math/MathCommon.h"
#include "ParticleData.h"
#include <string>
#include <random>

namespace UnoEngine {

class ParticleSystem;

class ParticleEmitter {
public:
    ParticleEmitter();
    explicit ParticleEmitter(const EmitterConfig& config);
    ~ParticleEmitter() = default;

    // 更新（CPU側の状態管理）
    void Update(float deltaTime);

    // 再生制御
    void Play();
    void Pause();
    void Stop();
    void Restart();

    // 設定アクセス
    EmitterConfig& GetConfig() { return config_; }
    const EmitterConfig& GetConfig() const { return config_; }
    void SetConfig(const EmitterConfig& config) { config_ = config; }

    // 状態取得
    bool IsPlaying() const { return isPlaying_; }
    bool IsPaused() const { return isPaused_; }
    float GetTime() const { return time_; }
    uint32 GetEmitterID() const { return emitterID_; }

    // 発生数計算
    uint32 CalculateEmitCount(float deltaTime);

    // Transform
    void SetPosition(const Float3& pos) { position_ = pos; }
    void SetRotation(const Float3& rot) { rotation_ = rot; }
    void SetScale(const Float3& scale) { scale_ = scale; }
    const Float3& GetPosition() const { return position_; }
    const Float3& GetRotation() const { return rotation_; }
    const Float3& GetScale() const { return scale_; }

    // パーティクル生成用の初期値サンプリング
    float SampleLifetime() const;
    float SampleSpeed() const;
    float SampleSize() const;
    Float4 SampleColor() const;
    float SampleRotation() const;

    // 形状からの位置/方向サンプリング
    void SampleShape(Float3& outPosition, Float3& outDirection) const;

private:
    void ProcessBursts(float deltaTime);

    EmitterConfig config_;
    uint32 emitterID_ = 0;

    // Transform
    Float3 position_ = { 0.0f, 0.0f, 0.0f };
    Float3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Float3 scale_ = { 1.0f, 1.0f, 1.0f };

    // 状態
    bool isPlaying_ = false;
    bool isPaused_ = false;
    float time_ = 0.0f;
    float emitAccumulator_ = 0.0f;

    // バースト管理
    std::vector<int32> burstCycleCount_;
    std::vector<float> burstNextTime_;

    // 乱数
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<float> uniformDist_{ 0.0f, 1.0f };

    static uint32 nextEmitterID_;

    friend class ParticleSystem;
};

} // namespace UnoEngine
