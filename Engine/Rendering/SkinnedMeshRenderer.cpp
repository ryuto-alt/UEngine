#include "SkinnedMeshRenderer.h"
#include "../Core/GameObject.h"
#include "../Core/Logger.h"
#include "../Animation/AnimatorComponent.h"
#include "../Graphics/SkinnedMesh.h"
#include <limits>

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
            Logger::Info("[コンポーネント] SkinnedMeshRenderer と Animator 連携完了");
        } else {
            Logger::Warning("[警告] SkinnedMeshRenderer: AnimatorComponent が見つかりません");
        }
    }
    
    // Initialize animator with model data if needed
    if (needsAnimatorInit_ && animator_ && modelData_) {
        InitializeAnimator();
    }
    
    Logger::Debug("[コンポーネント] SkinnedMeshRenderer 状態: モデル={}, Animator={}", 
              HasModel() ? "あり" : "なし", HasAnimator() ? "あり" : "なし");
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
        
        Logger::Info("[コンポーネント] SkinnedMeshRenderer モデル設定 (メッシュ: {}個)", modelData_->meshes.size());
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

    // 全メッシュのバウンディングボックスを結合
    Vector3 totalMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vector3 totalMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

    for (const auto& mesh : modelData_->meshes) {
        Vector3 meshMin = mesh.GetBoundsMin();
        Vector3 meshMax = mesh.GetBoundsMax();

        totalMin = Vector3(
            std::min(totalMin.GetX(), meshMin.GetX()),
            std::min(totalMin.GetY(), meshMin.GetY()),
            std::min(totalMin.GetZ(), meshMin.GetZ())
        );
        totalMax = Vector3(
            std::max(totalMax.GetX(), meshMax.GetX()),
            std::max(totalMax.GetY(), meshMax.GetY()),
            std::max(totalMax.GetZ(), meshMax.GetZ())
        );
    }

    // 有効なバウンディングボックスが計算されたか確認
    if (totalMin.GetX() <= totalMax.GetX() &&
        totalMin.GetY() <= totalMax.GetY() &&
        totalMin.GetZ() <= totalMax.GetZ()) {
        UpdateBounds(BoundingBox(totalMin, totalMax));
    } else {
        // フォールバック: デフォルトサイズ
        UpdateBounds(BoundingBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f)));
    }
}

} // namespace UnoEngine
