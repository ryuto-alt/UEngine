#pragma once
#include "ECS/System.h"

namespace ECS {

class EnemySenseSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "EnemySenseSystem"; }
};

} // namespace ECS
