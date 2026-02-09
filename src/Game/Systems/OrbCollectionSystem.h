#pragma once
#include "ECS/System.h"

namespace ECS {

// Checks sphere collision between player and orbs, handles collection
class OrbCollectionSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "OrbCollectionSystem"; }
};

} // namespace ECS
