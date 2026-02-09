#pragma once
#include "ECS/System.h"

namespace ECS {

// Checks if orbs are illuminated by the spotlight, adjusts glow intensity
class SpotLightGlowSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "SpotLightGlowSystem"; }
};

} // namespace ECS
