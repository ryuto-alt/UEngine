#pragma once
#include "ECS/System.h"

namespace ECS {

// Reads keyboard/gamepad input and writes to PlayerMovementComponent
class InputSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "InputSystem"; }
};

} // namespace ECS
