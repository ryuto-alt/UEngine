#pragma once
#include "ECS/System.h"

namespace ECS {

// Enemy detects player footstep sounds within range
class SoundDetectionSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "SoundDetectionSystem"; }
};

} // namespace ECS
