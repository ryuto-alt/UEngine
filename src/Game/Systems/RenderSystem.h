#pragma once
#include "ECS/System.h"

namespace ECS {

// Main render pass: PostProcess chain, skybox, objects, player, orbs, enemy
class RenderSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "RenderSystem"; }
};

} // namespace ECS
