#pragma once
#include "ECS/System.h"

namespace ECS {

// Subtitle-driven tutorial with movement lockout
class TutorialSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "TutorialSystem"; }
};

} // namespace ECS
