#pragma once
#include "ECS/System.h"

namespace ECS {

// Handles debug key inputs (F/M/V/TAB/F4/R)
class DebugInputSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "DebugInputSystem"; }
};

} // namespace ECS
