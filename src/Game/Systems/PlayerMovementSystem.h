#pragma once
#include "ECS/System.h"

namespace ECS {

// Camera-direction-based movement from input
class PlayerMovementSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "PlayerMovementSystem"; }
};

} // namespace ECS
