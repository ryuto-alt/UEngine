#include "LightDistributionSystem.h"
#include "ECS/World.h"
#include "ECS/Components/RenderComponents.h"
#include "UnoEngine.h"

namespace ECS {

void LightDistributionSystem::Update(World& world, float deltaTime) {
    auto* lightManager = world.GetResource<LightManager*>();
    if (!lightManager) return;

    const DirectionalLight& dirLight = lightManager->GetDirectionalLight();
    const SpotLight& spotLight = lightManager->GetSpotLight();

    // Apply lights to all mesh renderers
    world.ForEach<MeshRendererComponent>(
        [&dirLight, &spotLight](Entity entity, MeshRendererComponent& renderer) {
            if (!renderer.object3d) return;
            renderer.object3d->SetDirectionalLight(dirLight);
            renderer.object3d->SetSpotLight(spotLight);
        }
    );
}

} // namespace ECS
