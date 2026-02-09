#pragma once
#include "ECS/System.h"

namespace ECS {

// Smoothly interpolates currentRotationY toward targetRotationY
class RotationSmoothingSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "RotationSmoothingSystem"; }
};

} // namespace ECS
