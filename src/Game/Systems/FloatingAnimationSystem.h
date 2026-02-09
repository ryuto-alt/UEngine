#pragma once
#include "ECS/System.h"

namespace ECS {

// Sin-wave floating animation for Orbs
class FloatingAnimationSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "FloatingAnimationSystem"; }
};

} // namespace ECS
