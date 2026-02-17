#pragma once
#include "ECS/System.h"

namespace ECS {

class AmbushWarpSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "AmbushWarpSystem"; }
};

} // namespace ECS
