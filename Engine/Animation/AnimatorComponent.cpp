#include "AnimatorComponent.h"
#include "AnimationState.h"

namespace UnoEngine {

void AnimatorComponent::Initialize(std::shared_ptr<Skeleton> skeleton,
                                   const std::vector<std::shared_ptr<AnimationClip>>& clips) {
    if (!skeleton) {
        return;
    }

    animator_.SetSkeleton(skeleton);

    // Add all animation clips and create states for them
    for (const auto& clip : clips) {
        if (clip) {
            std::string clipName = clip->GetName();
            if (clipName.empty()) {
                clipName = "Animation_" + std::to_string(&clip - &clips[0]);
            }

            animator_.AddClip(clipName, clip);
            animator_.AddState(clipName, clipName);
        }
    }

    initialized_ = true;
}

void AnimatorComponent::Play(const std::string& animationName, bool loop) {
    if (!initialized_) {
        return;
    }

    AnimationState* state = animator_.GetState(animationName);
    if (state) {
        state->SetWrapMode(loop ? AnimationWrapMode::Loop : AnimationWrapMode::Once);
    }

    animator_.Play(animationName);
}

void AnimatorComponent::Stop() {
    animator_.Stop();
}

bool AnimatorComponent::IsPlaying() const {
    return animator_.IsPlaying();
}

const std::vector<Matrix4x4>& AnimatorComponent::GetBoneMatrices() const {
    return animator_.GetBoneMatrices();
}

const std::vector<BoneMatrixPair>& AnimatorComponent::GetBoneMatrixPairs() const {
    return animator_.GetBoneMatrixPairs();
}

uint32 AnimatorComponent::GetBoneCount() const {
    return animator_.GetBoneCount();
}

void AnimatorComponent::UpdateAnimation(float deltaTime) {
    if (!initialized_ || !IsEnabled()) {
        return;
    }

    animator_.OnUpdate(deltaTime);
}

} // namespace UnoEngine
