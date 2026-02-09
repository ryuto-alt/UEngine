#pragma once
#include "ECS/System.h"

namespace ECS {

// Enemy stealth footstep timer logic
class StealthSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "StealthSystem"; }
};

} // namespace ECS
