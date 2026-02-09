#pragma once
#include "ECS/System.h"

namespace ECS {

// Flashlight follows player position + camera rotation
class FlashlightSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "FlashlightSystem"; }
};

} // namespace ECS
