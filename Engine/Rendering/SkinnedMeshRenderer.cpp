#include "SkinnedMeshRenderer.h"
#include "../Core/GameObject.h"
#include "../Core/Logger.h"
#include "../Animation/AnimatorComponent.h"
#include "../Graphics/SkinnedMesh.h"

namespace UnoEngine {

void SkinnedMeshRenderer::Awake() {
    // Try to find AnimatorComponent on the same GameObject
    LinkAnimator();
}

void SkinnedMeshRenderer::Start() {
    // Re-attempt to link animator if not already linked
    // (AnimatorComponent might have been added after this component)
    if (!animator_) {
        LinkAnimator();
        if (animator_) {
            Logger::Info("SkinnedMeshRenderer::Start: Animator linked successfully");
        } else {
            Logger::Warning("SkinnedMeshRenderer::Start: No AnimatorComponent found on GameObject");
        }
    }
    
    // Initialize animator with model data if needed
    if (needsAnimatorInit_ && animator_ && modelData_) {
        InitializeAnimator();
    }
    
    Logger::Debug("SkinnedMeshRenderer::Start: HasModel={}, HasAnimator={}", HasModel(), HasAnimator());
}

void SkinnedMeshRenderer::OnDestroy() {
    modelData_ = nullptr;
    animator_ = nullptr;
}

void SkinnedMeshRenderer::SetModel(const std::string& path) {
    modelPath_ = path;
    // Note: Actual loading should be done via ResourceManager
    // This path is stored for deferred loading or serialization
    Logger::Info("SkinnedMeshRenderer: Model path set to: {}", path);
}

void SkinnedMeshRenderer::SetModel(SkinnedModelData* modelData) {
    modelData_ = modelData;
    
    if (modelData_) {
        // Set default material from first mesh if available
        if (!modelData_->meshes.empty() && modelData_->meshes[0].GetMaterial()) {
            SetDefaultMaterial(const_cast<Material*>(modelData_->meshes[0].GetMaterial()));
        }
        
        // Calculate bounds
        CalculateBounds();
        
        // If animator exists and model has skeleton, mark for initialization
        if (animator_ && modelData_->skeleton) {
            needsAnimatorInit_ = true;
        }
        
        Logger::Info("SkinnedMeshRenderer: Model set with {} meshes", modelData_->meshes.size());
    }
}

const std::vector<SkinnedMesh>& SkinnedMeshRenderer::GetMeshes() const {
    static const std::vector<SkinnedMesh> empty;
    return modelData_ ? modelData_->meshes : empty;
}

const std::vector<BoneMatrixPair>* SkinnedMeshRenderer::GetBoneMatrixPairs() const {
    if (animator_) {
        return &animator_->GetBoneMatrixPairs();
    }
    return nullptr;
}

void SkinnedMeshRenderer::LinkAnimator() {
    if (!GetGameObject()) return;
    
    animator_ = GetGameObject()->GetComponent<AnimatorComponent>();
    
    if (animator_) {
        Logger::Debug("SkinnedMeshRenderer: Linked to AnimatorComponent");
    }
}

void SkinnedMeshRenderer::InitializeAnimator() {
    if (!animator_ || !modelData_ || !modelData_->skeleton) return;
    
    animator_->Initialize(modelData_->skeleton, modelData_->animations);
    
    // Auto-play first animation if available
    if (!modelData_->animations.empty()) {
        std::string animName = modelData_->animations[0]->GetName();
        if (animName.empty()) {
            animName = "Animation_0";
        }
        animator_->Play(animName, true);
        Logger::Info("SkinnedMeshRenderer: Auto-playing animation: {}", animName);
    }
    
    needsAnimatorInit_ = false;
}

void SkinnedMeshRenderer::CalculateBounds() {
    if (!modelData_ || modelData_->meshes.empty()) return;
    
    BoundingBox combinedBounds;
    
    for (const auto& mesh : modelData_->meshes) {
        // Get vertex data to calculate bounds
        // For now, use a default bounding box
        // TODO: Calculate actual bounds from vertex data
    }
    
    // Use a reasonable default for now
    combinedBounds = BoundingBox(
        Vector3(-1.0f, -1.0f, -1.0f),
        Vector3(1.0f, 2.0f, 1.0f)
    );
    
    UpdateBounds(combinedBounds);
}

} // namespace UnoEngine
