#include "AnimationClip.h"
#include "Skeleton.h"
#include <algorithm>

namespace UnoEngine {

namespace {

template<typename T>
size_t FindKeyIndex(const std::vector<Keyframe<T>>& keys, float time) {
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (time < keys[i + 1].time) {
            return i;
        }
    }
    return keys.size() - 2;
}

float CalculateBlendFactor(float time, float t0, float t1) {
    float delta = t1 - t0;
    if (delta < 0.0001f) {
        return 0.0f;
    }
    return (time - t0) / delta;
}

} // anonymous namespace

Vector3 BoneAnimation::InterpolatePosition(float time) const {
    if (positionKeys.empty()) {
        return Vector3(0.0f, 0.0f, 0.0f);
    }
    if (positionKeys.size() == 1) {
        return positionKeys[0].value;
    }

    size_t index = FindKeyIndex(positionKeys, time);
    float factor = CalculateBlendFactor(time, positionKeys[index].time, positionKeys[index + 1].time);

    return Vector3::Lerp(positionKeys[index].value, positionKeys[index + 1].value, factor);
}

Quaternion BoneAnimation::InterpolateRotation(float time) const {
    if (rotationKeys.empty()) {
        return Quaternion::Identity();
    }
    if (rotationKeys.size() == 1) {
        return rotationKeys[0].value;
    }

    size_t index = FindKeyIndex(rotationKeys, time);
    float factor = CalculateBlendFactor(time, rotationKeys[index].time, rotationKeys[index + 1].time);

    return Quaternion::Slerp(rotationKeys[index].value, rotationKeys[index + 1].value, factor);
}

Vector3 BoneAnimation::InterpolateScale(float time) const {
    if (scaleKeys.empty()) {
        return Vector3(1.0f, 1.0f, 1.0f);
    }
    if (scaleKeys.size() == 1) {
        return scaleKeys[0].value;
    }

    size_t index = FindKeyIndex(scaleKeys, time);
    float factor = CalculateBlendFactor(time, scaleKeys[index].time, scaleKeys[index + 1].time);

    return Vector3::Lerp(scaleKeys[index].value, scaleKeys[index + 1].value, factor);
}

Matrix4x4 BoneAnimation::GetLocalTransform(float time) const {
    Vector3 position = InterpolatePosition(time);
    Quaternion rotation = InterpolateRotation(time);
    Vector3 scale = InterpolateScale(time);

    // DirectXMath: S * R * T の順序で構築（意図）
    Matrix4x4 S = Matrix4x4::CreateScale(scale);
    Matrix4x4 R = Matrix4x4::CreateFromQuaternion(rotation);
    Matrix4x4 T = Matrix4x4::CreateTranslation(position);

    return S * R * T;
}

void AnimationClip::AddBoneAnimation(const BoneAnimation& boneAnim) {
    boneNameToAnimIndex_[boneAnim.boneName] = boneAnimations_.size();
    boneAnimations_.push_back(boneAnim);
}

const BoneAnimation* AnimationClip::GetBoneAnimation(const std::string& boneName) const {
    auto it = boneNameToAnimIndex_.find(boneName);
    if (it != boneNameToAnimIndex_.end()) {
        return &boneAnimations_[it->second];
    }
    return nullptr;
}

void AnimationClip::Sample(float time, const Skeleton& skeleton,
                           std::vector<Matrix4x4>& outLocalTransforms) const {
    uint32 boneCount = skeleton.GetBoneCount();
    outLocalTransforms.resize(boneCount);

    for (uint32 i = 0; i < boneCount; ++i) {
        const Bone* bone = skeleton.GetBone(static_cast<int32>(i));
        if (!bone) continue;

        const BoneAnimation* anim = GetBoneAnimation(bone->name);
        if (anim) {
            outLocalTransforms[i] = anim->GetLocalTransform(time);
        } else {
            outLocalTransforms[i] = bone->localBindPose;
        }
    }
}

} // namespace UnoEngine
