#pragma once

#include "MeshRendererBase.h"
#include "../Resource/SkinnedModelImporter.h"
#include <string>
#include <vector>

namespace UnoEngine {

class SkinnedMesh;
class AnimatorComponent;
class ResourceManager;
struct BoneMatrixPair;

class SkinnedMeshRenderer : public MeshRendererBase {
public:
    SkinnedMeshRenderer() = default;
    ~SkinnedMeshRenderer() override = default;

    // Lifecycle
    void Awake() override;
    void Start() override;
    void OnDestroy() override;

    // Model loading via ResourceManager
    void SetModel(const std::string& path);
    void SetModel(SkinnedModelData* modelData);

    // Model access
    SkinnedModelData* GetModelData() const { return modelData_; }
    const std::vector<SkinnedMesh>& GetMeshes() const;
    bool HasModel() const { return modelData_ != nullptr && !modelData_->meshes.empty(); }

    // Bone matrices (from linked Animator)
    const std::vector<BoneMatrixPair>* GetBoneMatrixPairs() const;
    
    // Animator access (密結合)
    AnimatorComponent* GetAnimator() const { return animator_; }
    bool HasAnimator() const { return animator_ != nullptr; }

    // Get model path for serialization
    const std::string& GetModelPath() const { return modelPath_; }

private:
    void LinkAnimator();
    void InitializeAnimator();
    void CalculateBounds();

    SkinnedModelData* modelData_ = nullptr;
    AnimatorComponent* animator_ = nullptr;
    std::string modelPath_;
    bool needsAnimatorInit_ = false;
};

} // namespace UnoEngine
