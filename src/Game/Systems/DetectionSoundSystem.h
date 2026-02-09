#pragma once
#include "ECS/System.h"

namespace ECS {

// Enemy detection scream on chase start (stealth mode)
class DetectionSoundSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "DetectionSoundSystem"; }
};

} // namespace ECS
