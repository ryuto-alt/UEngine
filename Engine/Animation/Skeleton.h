#pragma once

#include "../Core/Types.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Math/Quaternion.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace UnoEngine {

constexpr uint32 MAX_BONES = 256;
constexpr int32 INVALID_BONE_INDEX = -1;

struct Bone {
    std::string name;
    int32 parentIndex = INVALID_BONE_INDEX;
    Matrix4x4 offsetMatrix;      // メッシュ空間からボーン空間への変換
    Matrix4x4 localBindPose;     // ローカルバインドポーズ
};

class Skeleton {
public:
    Skeleton() = default;
    ~Skeleton() = default;

    void AddBone(const std::string& name, int32 parentIndex,
                 const Matrix4x4& offsetMatrix, const Matrix4x4& localBindPose);

    void SetGlobalInverseTransform(const Matrix4x4& transform) { globalInverseTransform_ = transform; }
    const Matrix4x4& GetGlobalInverseTransform() const { return globalInverseTransform_; }

    int32 GetBoneIndex(const std::string& name) const;
    const Bone* GetBone(int32 index) const;
    const Bone* GetBone(const std::string& name) const;

    uint32 GetBoneCount() const { return static_cast<uint32>(bones_.size()); }
    const std::vector<Bone>& GetBones() const { return bones_; }

    void ComputeBoneMatrices(const std::vector<Matrix4x4>& localTransforms,
                             std::vector<Matrix4x4>& outFinalMatrices) const;

    void ComputeBindPoseMatrices(std::vector<Matrix4x4>& outFinalMatrices) const;

private:
    std::vector<Bone> bones_;
    std::unordered_map<std::string, int32> boneNameToIndex_;
    Matrix4x4 globalInverseTransform_;  // シーンルートノードの逆変換
};

} // namespace UnoEngine
