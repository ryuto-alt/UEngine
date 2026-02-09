#pragma once
#include "ECS/System.h"

namespace ECS {

// Updates animated models (blend timers, play/pause, advance time)
class AnimationSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "AnimationSystem"; }
};

} // namespace ECS
