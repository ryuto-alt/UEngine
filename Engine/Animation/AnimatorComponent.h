#pragma once

#include "../Core/Component.h"
#include "Animator.h"
#include "AnimationClip.h"
#include "Skeleton.h"
#include <memory>
#include <vector>

namespace UnoEngine {

class AnimatorComponent : public Component {
public:
    AnimatorComponent() = default;
    ~AnimatorComponent() override = default;

    // Initialize with skeleton and animation clips from SkinnedModelData
    void Initialize(std::shared_ptr<Skeleton> skeleton, 
                   const std::vector<std::shared_ptr<AnimationClip>>& clips);

    // Play animation by name
    void Play(const std::string& animationName, bool loop = true);
    
    // Stop current animation
    void Stop();
    
    // Check if animation is playing
    bool IsPlaying() const;

    // Get bone matrices for rendering
    const std::vector<Matrix4x4>& GetBoneMatrices() const;
    
    // Get bone matrix pairs (with inverse transpose for normals)
    const std::vector<BoneMatrixPair>& GetBoneMatrixPairs() const;
    
    // Get bone count
    uint32 GetBoneCount() const;

    // Access to underlying animator for advanced usage
    Animator* GetAnimator() { return &animator_; }
    const Animator* GetAnimator() const { return &animator_; }

    // Called by AnimationSystem
    void UpdateAnimation(float deltaTime);

private:
    Animator animator_;
    bool initialized_ = false;
};

} // namespace UnoEngine
