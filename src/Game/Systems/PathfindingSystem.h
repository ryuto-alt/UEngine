#pragma once
#include "ECS/System.h"

namespace ECS {

// NavMesh pathfinding: path calculation and following
class PathfindingSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "PathfindingSystem"; }
};

} // namespace ECS
