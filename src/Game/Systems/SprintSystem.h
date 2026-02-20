#pragma once
#include "ECS/System.h"

namespace ECS {

class SprintSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "SprintSystem"; }
};

} // namespace ECS
