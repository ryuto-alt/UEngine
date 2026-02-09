#pragma once
#include "ECS/System.h"

namespace ECS {

// Distributes directional+spot light to all renderable objects
class LightDistributionSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "LightDistributionSystem"; }
};

} // namespace ECS
