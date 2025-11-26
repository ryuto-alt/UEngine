#include "AnimatorComponent.h"
#include "AnimationState.h"
#include <Windows.h>

namespace UnoEngine {

void AnimatorComponent::Initialize(std::shared_ptr<Skeleton> skeleton,
                                   const std::vector<std::shared_ptr<AnimationClip>>& clips) {
    if (!skeleton) {
        OutputDebugStringA("AnimatorComponent::Initialize - No skeleton provided\n");
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

            char debugMsg[256];
            sprintf_s(debugMsg, "AnimatorComponent: Added clip '%s' (duration: %.2f)\n",
                     clipName.c_str(), clip->GetDuration() / clip->GetTicksPerSecond());
            OutputDebugStringA(debugMsg);
        }
    }

    initialized_ = true;

    char debugMsg[256];
    sprintf_s(debugMsg, "AnimatorComponent initialized: %u bones, %zu clips\n",
             skeleton->GetBoneCount(), clips.size());
    OutputDebugStringA(debugMsg);
}

void AnimatorComponent::Play(const std::string& animationName, bool loop) {
    if (!initialized_) {
        OutputDebugStringA("AnimatorComponent::Play - Not initialized\n");
        return;
    }

    AnimationState* state = animator_.GetState(animationName);
    if (state) {
        state->SetWrapMode(loop ? AnimationWrapMode::Loop : AnimationWrapMode::Once);
    }
    
    animator_.Play(animationName);

    char debugMsg[256];
    sprintf_s(debugMsg, "AnimatorComponent: Playing '%s' (loop: %s)\n",
             animationName.c_str(), loop ? "true" : "false");
    OutputDebugStringA(debugMsg);
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
