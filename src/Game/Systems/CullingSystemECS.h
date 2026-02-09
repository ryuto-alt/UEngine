#pragma once
#include "ECS/System.h"

namespace ECS {

// Frustum + distance culling, visibility assignment
class CullingSystemECS final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "CullingSystemECS"; }
};

} // namespace ECS
