#pragma once
#include "ECS/System.h"

namespace ECS {

// Fade out/in respawn with position reset
class RespawnSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "RespawnSystem"; }
};

} // namespace ECS
