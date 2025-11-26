#pragma once

#include "../Core/Types.h"
#include <string>
#include <memory>
#include <functional>

namespace UnoEngine {

class AnimationClip;

enum class AnimationWrapMode {
    Once,       // 1回再生して停止
    Loop,       // ループ再生
    PingPong,   // 往復再生
    ClampForever // 最後のフレームで停止
};

struct AnimationTransition {
    std::string targetStateName;
    float duration = 0.2f;  // ブレンド時間（秒）
    std::function<bool()> condition;
};

class AnimationState {
public:
    AnimationState() = default;
    AnimationState(const std::string& name, std::shared_ptr<AnimationClip> clip);
    ~AnimationState() = default;

    const std::string& GetName() const { return name_; }
    AnimationClip* GetClip() const { return clip_.get(); }
    std::shared_ptr<AnimationClip> GetClipShared() const { return clip_; }

    void SetWrapMode(AnimationWrapMode mode) { wrapMode_ = mode; }
    AnimationWrapMode GetWrapMode() const { return wrapMode_; }

    void SetSpeed(float speed) { speed_ = speed; }
    float GetSpeed() const { return speed_; }

    void AddTransition(const AnimationTransition& transition);
    const std::vector<AnimationTransition>& GetTransitions() const { return transitions_; }

    float GetNormalizedTime() const { return normalizedTime_; }
    void SetNormalizedTime(float t) { normalizedTime_ = t; }

    void Update(float deltaTime);
    float GetCurrentTime() const;
    bool IsFinished() const { return isFinished_; }

    void Reset();

private:
    std::string name_;
    std::shared_ptr<AnimationClip> clip_;
    AnimationWrapMode wrapMode_ = AnimationWrapMode::Loop;
    float speed_ = 1.0f;
    float normalizedTime_ = 0.0f;
    bool isFinished_ = false;

    std::vector<AnimationTransition> transitions_;
};

} // namespace UnoEngine
