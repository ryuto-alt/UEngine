#pragma once

#include "../Graphics/SkinnedMesh.h"
#include "../Graphics/Material.h"
#include "../Math/Matrix.h"
#include "../Animation/Skeleton.h"
#include <vector>

namespace UnoEngine {

class Animator;

struct SkinnedRenderItem {
    SkinnedMesh* mesh = nullptr;
    Material* material = nullptr;
    Matrix4x4 worldMatrix;
    const std::vector<Matrix4x4>* boneMatrices = nullptr;
    const std::vector<BoneMatrixPair>* boneMatrixPairs = nullptr;

    SkinnedRenderItem() = default;
    SkinnedRenderItem(SkinnedMesh* m, Material* mat, const Matrix4x4& world, const std::vector<Matrix4x4>* bones)
        : mesh(m), material(mat), worldMatrix(world), boneMatrices(bones), boneMatrixPairs(nullptr) {}
    SkinnedRenderItem(SkinnedMesh* m, Material* mat, const Matrix4x4& world, const std::vector<BoneMatrixPair>* bonePairs)
        : mesh(m), material(mat), worldMatrix(world), boneMatrices(nullptr), boneMatrixPairs(bonePairs) {}
};

} // namespace UnoEngine
