#pragma once
#include "ECS/System.h"

namespace ECS {

// FPS/orbit camera update from player position
class CameraSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "CameraSystem"; }
};

} // namespace ECS
