#pragma once

#include "../Core/Scene.h"
#include "../Core/Camera.h"
#include "RenderView.h"
#include "RenderItem.h"
#include "../Math/Matrix.h"
#include <vector>

namespace UnoEngine {

class RenderSystem {
public:
    RenderSystem() = default;
    ~RenderSystem() = default;

    std::vector<RenderItem> CollectRenderables(Scene* scene, const RenderView& view);

private:
    bool PassesLayerMask(uint32 objectLayer, uint32 viewMask) const;
};

} // namespace UnoEngine
