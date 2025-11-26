#pragma once

#include "../Core/Component.h"
#include "../Core/Types.h"
#include "../Math/Matrix.h"
#include "AnimationState.h"
#include "Skeleton.h"
#include "AnimationClip.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace UnoEngine {

class Animator : public Component {
public:
    Animator() = default;
    ~Animator() override = default;

    void OnUpdate(float deltaTime) override;

    void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
    Skeleton* GetSkeleton() const { return skeleton_.get(); }

    void AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip);
    AnimationClip* GetClip(const std::string& name) const;

    AnimationState* AddState(const std::string& stateName, const std::string& clipName);
    AnimationState* GetState(const std::string& stateName);
    AnimationState* GetCurrentState() const { return currentState_; }

    void Play(const std::string& stateName, float transitionDuration = 0.0f);
    void CrossFade(const std::string& stateName, float duration);
    void Stop();

    bool IsPlaying() const { return isPlaying_; }
    float GetCurrentTime() const;
    float GetNormalizedTime() const;

    const std::vector<Matrix4x4>& GetBoneMatrices() const { return finalBoneMatrices_; }
    uint32 GetBoneCount() const { return skeleton_ ? skeleton_->GetBoneCount() : 0; }

    void SetParameter(const std::string& name, float value);
    void SetParameter(const std::string& name, int32 value);
    void SetParameter(const std::string& name, bool value);

    float GetFloatParameter(const std::string& name) const;
    int32 GetIntParameter(const std::string& name) const;
    bool GetBoolParameter(const std::string& name) const;

private:
    void UpdateBoneMatrices();
    void CheckTransitions();
    void BlendAnimations(float blendFactor);

    std::shared_ptr<Skeleton> skeleton_;
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> clips_;
    std::unordered_map<std::string, std::unique_ptr<AnimationState>> states_;

    AnimationState* currentState_ = nullptr;
    AnimationState* nextState_ = nullptr;

    float transitionDuration_ = 0.0f;
    float transitionTime_ = 0.0f;
    bool isTransitioning_ = false;
    bool isPlaying_ = false;

    std::vector<Matrix4x4> finalBoneMatrices_;
    std::vector<Matrix4x4> currentLocalTransforms_;
    std::vector<Matrix4x4> nextLocalTransforms_;

    std::unordered_map<std::string, float> floatParams_;
    std::unordered_map<std::string, int32> intParams_;
    std::unordered_map<std::string, bool> boolParams_;
};

} // namespace UnoEngine
