#include "Skeleton.h"
#include <Windows.h>

namespace UnoEngine {

static bool debugPrinted = false;
static bool detailDebugPrinted = false;

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

    // デバッグ: 最初の1回だけ入力値を確認
    if (!debugPrinted && boneCount > 0) {
        debugPrinted = true;
        char msg[512];

        // GlobalInverseTransform
        sprintf_s(msg, "GlobalInvTrans row0: [%.3f, %.3f, %.3f, %.3f]\n",
                 globalInverseTransform_.GetElement(0,0), globalInverseTransform_.GetElement(0,1),
                 globalInverseTransform_.GetElement(0,2), globalInverseTransform_.GetElement(0,3));
        OutputDebugStringA(msg);

        // Bone0のOffsetMatrix
        sprintf_s(msg, "Bone0 Offset row0: [%.3f, %.3f, %.3f, %.3f]\n",
                 bones_[0].offsetMatrix.GetElement(0,0), bones_[0].offsetMatrix.GetElement(0,1),
                 bones_[0].offsetMatrix.GetElement(0,2), bones_[0].offsetMatrix.GetElement(0,3));
        OutputDebugStringA(msg);

        // LocalTransform[0] - 回転行列の最初の行
        sprintf_s(msg, "Local[0] row0: [%.3f, %.3f, %.3f, %.3f]\n",
                 localTransforms[0].GetElement(0,0), localTransforms[0].GetElement(0,1),
                 localTransforms[0].GetElement(0,2), localTransforms[0].GetElement(0,3));
        OutputDebugStringA(msg);

        // LocalTransform[0] - 移動成分
        sprintf_s(msg, "Local[0] row3 (translation): [%.3f, %.3f, %.3f, %.3f]\n",
                 localTransforms[0].GetElement(3,0), localTransforms[0].GetElement(3,1),
                 localTransforms[0].GetElement(3,2), localTransforms[0].GetElement(3,3));
        OutputDebugStringA(msg);
    }

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

        // 最初の2つのボーンの詳細デバッグ
        if (!detailDebugPrinted && i <= 1) {
            char msg[512];
            sprintf_s(msg, "Bone%d (parent=%d) - Local[3]=[%.3f,%.3f,%.3f], Global[3]=[%.3f,%.3f,%.3f], Final[0,0]=%.3f\n",
                     i, bone.parentIndex,
                     localTransforms[i].GetElement(3,0), localTransforms[i].GetElement(3,1), localTransforms[i].GetElement(3,2),
                     globalTransforms[i].GetElement(3,0), globalTransforms[i].GetElement(3,1), globalTransforms[i].GetElement(3,2),
                     outFinalMatrices[i].GetElement(0,0));
            OutputDebugStringA(msg);
            if (i == 1) detailDebugPrinted = true;
        }
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
