#pragma once

#include "../Core/Scene.h"
#include "../Core/Camera.h"
#include "RenderView.h"
#include "RenderItem.h"
#include "SkinnedRenderItem.h"
#include "../Math/Matrix.h"
#include <vector>

namespace UnoEngine {

class SkinnedMeshRenderer;

class RenderSystem {
public:
    RenderSystem() = default;
    ~RenderSystem() = default;

    // Collect static mesh renderables
    std::vector<RenderItem> CollectRenderables(Scene* scene, const RenderView& view);

    // Collect skinned mesh renderables (NEW)
    std::vector<SkinnedRenderItem> CollectSkinnedRenderables(Scene* scene, const RenderView& view);

    // Clear cached items
    void Clear();

private:
    bool PassesLayerMask(uint32 objectLayer, uint32 viewMask) const;
};

} // namespace UnoEngine
