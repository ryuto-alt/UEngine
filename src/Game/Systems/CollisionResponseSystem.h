#pragma once
#include "ECS/System.h"

namespace ECS {

// AABB pushout collision response for player
class CollisionResponseSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "CollisionResponseSystem"; }
};

} // namespace ECS
