#pragma once
#include "ECS/System.h"

namespace ECS {

// Ground collision check: prevents entities from falling through Y=0 plane
class GroundCollisionSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "GroundCollisionSystem"; }
};

} // namespace ECS
