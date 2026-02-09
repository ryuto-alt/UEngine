#pragma once
#include "ECS/System.h"

namespace ECS {

// LightManager update (flicker, blink timers)
class LightingSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "LightingSystem"; }
};

} // namespace ECS
