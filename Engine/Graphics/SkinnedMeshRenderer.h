#pragma once

#include "../Core/Component.h"
#include "../Animation/Animator.h"
#include "SkinnedMesh.h"
#include "Material.h"
#include <vector>
#include <memory>

namespace UnoEngine {

class SkinnedMeshRenderer : public Component {
public:
    SkinnedMeshRenderer() = default;
    ~SkinnedMeshRenderer() override = default;

    void SetMeshes(std::vector<SkinnedMesh>&& meshes) { meshes_ = std::move(meshes); }
    void SetAnimator(Animator* animator) { animator_ = animator; }

    const std::vector<SkinnedMesh>& GetMeshes() const { return meshes_; }
    SkinnedMesh* GetMesh(size_t index) { return index < meshes_.size() ? &meshes_[index] : nullptr; }
    const SkinnedMesh* GetMesh(size_t index) const { return index < meshes_.size() ? &meshes_[index] : nullptr; }
    size_t GetMeshCount() const { return meshes_.size(); }

    Animator* GetAnimator() const { return animator_; }
    const std::vector<Matrix4x4>* GetBoneMatrices() const {
        return animator_ ? &animator_->GetBoneMatrices() : nullptr;
    }
    
    const std::vector<BoneMatrixPair>* GetBoneMatrixPairs() const {
        return animator_ ? &animator_->GetBoneMatrixPairs() : nullptr;
    }

private:
    std::vector<SkinnedMesh> meshes_;
    Animator* animator_ = nullptr;
};

} // namespace UnoEngine
