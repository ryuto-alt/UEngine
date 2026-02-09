#pragma once
#include "ECS/System.h"

namespace ECS {

// Updates spatial audio listener position/orientation from player+camera
class AudioListenerSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "AudioListenerSystem"; }
};

} // namespace ECS
