#include "pch.h"
#include "RenderSystem.h"
#include "SkinnedMeshRenderer.h"
#include "../Graphics/MeshRenderer.h"
#include "../Graphics/SkinnedMesh.h"
#include "../Animation/AnimatorComponent.h"
#include "../Core/Logger.h"
#include "../Math/Math.h"
#include <algorithm>
#include <cassert>

namespace UnoEngine {

std::vector<RenderItem> RenderSystem::CollectRenderables(Scene* scene, const RenderView& view) {
    assert(scene && "Scene is null");
    assert(view.camera && "Camera is null");
    
    std::vector<RenderItem> items;
    
    for (const auto& go : scene->GetGameObjects()) {
        if (!go->IsActive()) continue;
        
        if (!PassesLayerMask(go->GetLayer(), view.layerMask)) continue;
        
        auto* meshRenderer = go->GetComponent<MeshRenderer>();
        if (!meshRenderer) continue;
        
        RenderItem item;
        item.mesh = meshRenderer->GetMesh();
        item.material = meshRenderer->GetMaterial();
        item.worldMatrix = go->GetTransform().GetWorldMatrix();
        
        items.push_back(item);
    }
    
    // Sort by material for batching
    std::sort(items.begin(), items.end(), 
        [](const RenderItem& a, const RenderItem& b) {
            return a.material < b.material;
        });
    
    return items;
}

std::vector<SkinnedRenderItem> RenderSystem::CollectSkinnedRenderables(Scene* scene, const RenderView& view) {
    assert(scene && "Scene is null");
    assert(view.camera && "Camera is null");
    
    std::vector<SkinnedRenderItem> items;
    
    for (const auto& go : scene->GetGameObjects()) {
        if (!go->IsActive()) continue;
        
        if (!PassesLayerMask(go->GetLayer(), view.layerMask)) continue;
        
        auto* skinnedRenderer = go->GetComponent<SkinnedMeshRenderer>();
        if (!skinnedRenderer) continue;
        
        if (!skinnedRenderer->HasModel()) {
            Logger::Warning("[描画] '{}' の SkinnedMeshRenderer にモデルがありません", go->GetName());
            continue;
        }
        
        // Get bone matrices from animator
        const std::vector<BoneMatrixPair>* bonePairs = skinnedRenderer->GetBoneMatrixPairs();
        
        // Get world matrix with coordinate system correction
        // glTF models often need rotation to stand up
        Matrix4x4 standUpRotation = Matrix4x4::RotationX(Math::PI / 2.0f);
        Matrix4x4 worldMatrix = standUpRotation * go->GetTransform().GetWorldMatrix();
        
        // Create render item for each mesh
        const auto& meshes = skinnedRenderer->GetMeshes();
        Logger::Debug("[描画] '{}' から {}個のメッシュを収集", go->GetName(), meshes.size());
        
        for (const auto& mesh : meshes) {
            SkinnedRenderItem item;
            item.mesh = const_cast<SkinnedMesh*>(&mesh);
            item.worldMatrix = worldMatrix;
            item.material = skinnedRenderer->GetMaterial();
            
            // Fallback to mesh's own material if renderer doesn't have one
            if (!item.material) {
                item.material = const_cast<Material*>(mesh.GetMaterial());
            }
            
            item.boneMatrixPairs = bonePairs;
            
            // デバッグ描画用にAnimatorを設定
            if (auto* animatorComp = skinnedRenderer->GetAnimator()) {
                item.animator = animatorComp->GetAnimator();
            }
            
            items.push_back(item);
        }
    }
    
    Logger::Debug("[描画] スキンメッシュ合計 {}個 収集完了", items.size());
    
    // Sort by material for batching
    std::sort(items.begin(), items.end(),
        [](const SkinnedRenderItem& a, const SkinnedRenderItem& b) {
            return a.material < b.material;
        });
    
    return items;
}

void RenderSystem::Clear() {
    // Reserved for future cached data clearing
}

bool RenderSystem::PassesLayerMask(uint32 objectLayer, uint32 viewMask) const {
    return (objectLayer & viewMask) != 0;
}

} // namespace UnoEngine
