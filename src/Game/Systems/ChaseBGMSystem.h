#pragma once
#include "ECS/System.h"

namespace ECS {

// Chase BGM fade in/out control
class ChaseBGMSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "ChaseBGMSystem"; }
};

} // namespace ECS
