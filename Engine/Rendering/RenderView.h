#pragma once

#include "../Core/Camera.h"
#include "../Core/Types.h"
#include <string>

namespace UnoEngine {

struct RenderView {
    Camera* camera = nullptr;
    uint32 layerMask = 0xFFFFFFFF;
    std::string viewName = "Main";
};

} // namespace UnoEngine
