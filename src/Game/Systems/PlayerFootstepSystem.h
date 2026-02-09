#pragma once
#include "ECS/System.h"

namespace ECS {

// Tracks player footstep timing for enemy sound detection
class PlayerFootstepSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "PlayerFootstepSystem"; }
};

} // namespace ECS
