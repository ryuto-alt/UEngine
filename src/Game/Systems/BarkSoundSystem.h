#pragma once
#include "ECS/System.h"

namespace ECS {

// Enemy bark sound during chase (initial + periodic)
class BarkSoundSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "BarkSoundSystem"; }
};

} // namespace ECS
