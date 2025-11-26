#include "Skeleton.h"
#include <Windows.h>

namespace UnoEngine {

static bool debugPrinted = false;

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

        // LocalTransform[0]
        sprintf_s(msg, "Local[0] row0: [%.3f, %.3f, %.3f, %.3f]\n",
                 localTransforms[0].GetElement(0,0), localTransforms[0].GetElement(0,1),
                 localTransforms[0].GetElement(0,2), localTransforms[0].GetElement(0,3));
        OutputDebugStringA(msg);
    }

    for (uint32 i = 0; i < boneCount; ++i) {
        const Bone& bone = bones_[i];

        // DirectX行ベクトル規約: Child = Parent * Local
        if (bone.parentIndex == INVALID_BONE_INDEX) {
            globalTransforms[i] = localTransforms[i];
        } else {
            globalTransforms[i] = globalTransforms[bone.parentIndex] * localTransforms[i];
        }

        // glTF用: FinalMatrix = GlobalTransform * OffsetMatrix
        // (GlobalInverseTransformは単位行列に設定済み)
        outFinalMatrices[i] = globalTransforms[i] * bone.offsetMatrix;
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
