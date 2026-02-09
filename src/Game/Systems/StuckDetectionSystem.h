#pragma once
#include "ECS/System.h"

namespace ECS {

// Detects when enemy is stuck and triggers recovery
class StuckDetectionSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "StuckDetectionSystem"; }
};

} // namespace ECS
