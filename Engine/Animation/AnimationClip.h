#pragma once

#include "../Core/Types.h"
#include "../Math/Vector.h"
#include "../Math/Quaternion.h"
#include "../Math/Matrix.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace UnoEngine {

template<typename T>
struct Keyframe {
    float time;
    T value;
};

struct BoneAnimation {
    std::string boneName;
    std::vector<Keyframe<Vector3>> positionKeys;
    std::vector<Keyframe<Quaternion>> rotationKeys;
    std::vector<Keyframe<Vector3>> scaleKeys;

    Vector3 InterpolatePosition(float time) const;
    Quaternion InterpolateRotation(float time) const;
    Vector3 InterpolateScale(float time) const;

    Matrix4x4 GetLocalTransform(float time) const;
};

class AnimationClip {
public:
    AnimationClip() = default;
    ~AnimationClip() = default;

    void SetName(const std::string& name) { name_ = name; }
    const std::string& GetName() const { return name_; }

    void SetDuration(float duration) { duration_ = duration; }
    float GetDuration() const { return duration_; }

    void SetTicksPerSecond(float tps) { ticksPerSecond_ = tps; }
    float GetTicksPerSecond() const { return ticksPerSecond_; }

    void AddBoneAnimation(const BoneAnimation& boneAnim);
    const BoneAnimation* GetBoneAnimation(const std::string& boneName) const;

    const std::vector<BoneAnimation>& GetBoneAnimations() const { return boneAnimations_; }

    void Sample(float time, const class Skeleton& skeleton,
                std::vector<Matrix4x4>& outLocalTransforms) const;

private:
    std::string name_;
    float duration_ = 0.0f;
    float ticksPerSecond_ = 25.0f;

    std::vector<BoneAnimation> boneAnimations_;
    std::unordered_map<std::string, size_t> boneNameToAnimIndex_;
};

} // namespace UnoEngine
