#pragma once
#include "ECS/System.h"

namespace ECS {

// Distance-based vignette, camera shake, light flicker during chase
class FearEffectSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "FearEffectSystem"; }
};

} // namespace ECS
