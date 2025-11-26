#include "AnimationState.h"
#include "AnimationClip.h"
#include <cmath>

namespace UnoEngine {

AnimationState::AnimationState(const std::string& name, std::shared_ptr<AnimationClip> clip)
    : name_(name), clip_(clip) {
}

void AnimationState::AddTransition(const AnimationTransition& transition) {
    transitions_.push_back(transition);
}

void AnimationState::Update(float deltaTime) {
    if (!clip_ || isFinished_) {
        return;
    }

    float duration = clip_->GetDuration();
    if (duration <= 0.0f) {
        return;
    }

    float normalizedDelta = (deltaTime * speed_) / duration;
    normalizedTime_ += normalizedDelta;

    switch (wrapMode_) {
    case AnimationWrapMode::Once:
        if (normalizedTime_ >= 1.0f) {
            normalizedTime_ = 1.0f;
            isFinished_ = true;
        }
        break;

    case AnimationWrapMode::Loop:
        normalizedTime_ = std::fmod(normalizedTime_, 1.0f);
        if (normalizedTime_ < 0.0f) {
            normalizedTime_ += 1.0f;
        }
        break;

    case AnimationWrapMode::PingPong: {
        float pingPongTime = std::fmod(normalizedTime_, 2.0f);
        if (pingPongTime < 0.0f) {
            pingPongTime += 2.0f;
        }
        if (pingPongTime > 1.0f) {
            normalizedTime_ = 2.0f - pingPongTime;
        } else {
            normalizedTime_ = pingPongTime;
        }
        break;
    }

    case AnimationWrapMode::ClampForever:
        if (normalizedTime_ >= 1.0f) {
            normalizedTime_ = 1.0f;
        } else if (normalizedTime_ < 0.0f) {
            normalizedTime_ = 0.0f;
        }
        break;
    }
}

float AnimationState::GetCurrentTime() const {
    if (!clip_) {
        return 0.0f;
    }
    return normalizedTime_ * clip_->GetDuration();
}

void AnimationState::Reset() {
    normalizedTime_ = 0.0f;
    isFinished_ = false;
}

} // namespace UnoEngine
