#pragma once
#include "ECS/System.h"

namespace ECS {

// Enemy AI state machine: patrol/search/chase animation control
class AIBehaviorSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "AIBehaviorSystem"; }
};

} // namespace ECS
