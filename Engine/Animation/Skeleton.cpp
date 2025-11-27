#include "Skeleton.h"

namespace UnoEngine {

void Skeleton::AddBone(const std::string& name, int32 parentIndex,
                       const Matrix4x4& offsetMatrix, const Matrix4x4& localBindPose) {
    int32 index = static_cast<int32>(bones_.size());

    Bone bone;
    bone.name = name;
    bone.parentIndex = parentIndex;
    bone.offsetMatrix = offsetMatrix;
    bone.localBindPose = localBindPose;

    bones_.push_back(bone);
    boneNameToIndex_[name] = index;
}

int32 Skeleton::GetBoneIndex(const std::string& name) const {
    auto it = boneNameToIndex_.find(name);
    if (it != boneNameToIndex_.end()) {
        return it->second;
    }
    return INVALID_BONE_INDEX;
}

const Bone* Skeleton::GetBone(int32 index) const {
    if (index >= 0 && index < static_cast<int32>(bones_.size())) {
        return &bones_[index];
    }
    return nullptr;
}

const Bone* Skeleton::GetBone(const std::string& name) const {
    int32 index = GetBoneIndex(name);
    return GetBone(index);
}

void Skeleton::ComputeBoneMatrices(const std::vector<Matrix4x4>& localTransforms,
                                   std::vector<Matrix4x4>& outFinalMatrices) const {
    const uint32 boneCount = GetBoneCount();
    outFinalMatrices.resize(boneCount);

    std::vector<Matrix4x4> globalTransforms(boneCount);

    for (uint32 i = 0; i < boneCount; ++i) {
        const Bone& bone = bones_[i];

        // prohと同じ: Global = Local * Parent
        if (bone.parentIndex == INVALID_BONE_INDEX) {
            globalTransforms[i] = localTransforms[i];
        } else {
            globalTransforms[i] = localTransforms[i] * globalTransforms[bone.parentIndex];
        }

        // prohと同じ: Final = InverseBindPose * Global
        outFinalMatrices[i] = bone.offsetMatrix * globalTransforms[i];
    }
}

void Skeleton::ComputeBoneMatricesWithInverseTranspose(const std::vector<Matrix4x4>& localTransforms,
                                                       std::vector<BoneMatrixPair>& outBoneMatrices) const {
    const uint32 boneCount = GetBoneCount();
    outBoneMatrices.resize(boneCount);

    std::vector<Matrix4x4> globalTransforms(boneCount);

    for (uint32 i = 0; i < boneCount; ++i) {
        const Bone& bone = bones_[i];

        // prohと同じ: Global = Local * Parent
        if (bone.parentIndex == INVALID_BONE_INDEX) {
            globalTransforms[i] = localTransforms[i];
        } else {
            globalTransforms[i] = localTransforms[i] * globalTransforms[bone.parentIndex];
        }

        // prohと同じ: Final = InverseBindPose * Global
        outBoneMatrices[i].skeletonSpaceMatrix = bone.offsetMatrix * globalTransforms[i];

        // InverseTranspose行列を計算（法線変換用）
        outBoneMatrices[i].skeletonSpaceInverseTransposeMatrix =
            outBoneMatrices[i].skeletonSpaceMatrix.Inverse().Transpose();
    }
}

void Skeleton::ComputeBindPoseMatrices(std::vector<Matrix4x4>& outFinalMatrices) const {
    // バインドポーズでは各ボーンのローカルバインドポーズを使用
    std::vector<Matrix4x4> localTransforms(GetBoneCount());
    for (uint32 i = 0; i < GetBoneCount(); ++i) {
        localTransforms[i] = bones_[i].localBindPose;
    }
    ComputeBoneMatrices(localTransforms, outFinalMatrices);
}

} // namespace UnoEngine
