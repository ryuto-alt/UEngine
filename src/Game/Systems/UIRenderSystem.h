#pragma once
#include "ECS/System.h"

namespace ECS {

// Minimap, subtitles, fade sprite rendering
class UIRenderSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "UIRenderSystem"; }
};

} // namespace ECS
