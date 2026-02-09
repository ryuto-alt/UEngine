#pragma once
#include "ECS/System.h"

namespace ECS {

// AABB collision check, camera control, light override during jumpscare
class JumpscareSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "JumpscareSystem"; }
};

} // namespace ECS
