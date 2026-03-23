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

    // Collect static mesh renderables (returns cached ref, valid until next call)
    const std::vector<RenderItem>& CollectRenderables(Scene* scene, const RenderView& view);

    // Collect skinned mesh renderables (returns cached ref, valid until next call)
    const std::vector<SkinnedRenderItem>& CollectSkinnedRenderables(Scene* scene, const RenderView& view);

    void Clear();

private:
    bool PassesLayerMask(uint32 objectLayer, uint32 viewMask) const;

    std::vector<RenderItem> cachedItems_;
    std::vector<SkinnedRenderItem> cachedSkinnedItems_;
};

} // namespace UnoEngine
