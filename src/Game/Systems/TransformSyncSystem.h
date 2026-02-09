#pragma once
#include "ECS/System.h"

namespace ECS {

// Bridges ECS TransformComponent data into the existing Object3d rendering pipeline
class TransformSyncSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "TransformSyncSystem"; }
};

} // namespace ECS
