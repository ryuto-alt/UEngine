#pragma once

#include "Scene.h"
#include "Camera.h"
#include "../Graphics/RenderView.h"
#include "../Graphics/RenderItem.h"
#include "../Math/Matrix.h"
#include <vector>

namespace UnoEngine {

// Rendering system
class RenderSystem {
public:
    RenderSystem() = default;
    ~RenderSystem() = default;

    std::vector<RenderItem> CollectRenderables(Scene* scene, const RenderView& view);

private:
    bool PassesLayerMask(uint32 objectLayer, uint32 viewMask) const;

};

} // namespace UnoEngine
