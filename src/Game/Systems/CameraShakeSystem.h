#pragma once
#include "ECS/System.h"

namespace ECS {

// Walk/run/fear-based camera shake
class CameraShakeSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "CameraShakeSystem"; }
};

} // namespace ECS
