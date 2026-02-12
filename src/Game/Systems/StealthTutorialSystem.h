#pragma once
#include "ECS/System.h"

namespace ECS {

// Shows a one-time subtitle when enemy first enters stealth mode
class StealthTutorialSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "StealthTutorialSystem"; }
};

} // namespace ECS
