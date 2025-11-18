#include "RenderSystem.h"
#include "../Graphics/MeshRenderer.h"
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
    
    std::sort(items.begin(), items.end(), 
        [](const RenderItem& a, const RenderItem& b) {
            return a.material < b.material;
        });
    
    return items;
}

bool RenderSystem::PassesLayerMask(uint32 objectLayer, uint32 viewMask) const {
    return (objectLayer & viewMask) != 0;
}



} // namespace UnoEngine
