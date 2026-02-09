#include "LightingSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"

namespace ECS {

void LightingSystem::Update(World& world, float deltaTime) {
    auto* lightManager = world.GetResource<LightManager*>();
    if (!lightManager) return;

    lightManager->Update(deltaTime);
}

} // namespace ECS
