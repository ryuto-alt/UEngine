#pragma once
#include "ECS/System.h"

namespace ECS {

// Enemy vision cone + proximity detection with NavMesh raycast
class VisionSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "VisionSystem"; }
};

} // namespace ECS
