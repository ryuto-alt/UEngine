#pragma once
#include "ECS/System.h"

namespace ECS {

// position += velocity * deltaTime for all entities
class VelocityIntegrationSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "VelocityIntegrationSystem"; }
};

} // namespace ECS
