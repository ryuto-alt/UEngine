#pragma once
#include "ECS/System.h"

namespace ECS {

// Applies gravity to all entities with VelocityComponent + GravityComponent
class GravitySystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "GravitySystem"; }
};

} // namespace ECS
