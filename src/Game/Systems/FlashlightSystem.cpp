#include "FlashlightSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/CameraComponents.h"
#include "UnoEngine.h"
#include "GameObject/FPSCamera.h"

namespace ECS {

void FlashlightSystem::Update(World& world, float deltaTime) {
    auto* lightManager = world.GetResource<LightManager*>();
    if (!lightManager) return;

    world.ForEach<PlayerTag, TransformComponent, FPSCameraComponent>(
        [lightManager](Entity entity, PlayerTag&, TransformComponent& transform,
                      FPSCameraComponent& fpsCam) {
            if (!fpsCam.fpsCamera) return;

            Vector3 cameraRotation = fpsCam.fpsCamera->GetCameraRotation();
            lightManager->UpdateFlashlight(transform.position, cameraRotation);
        }
    );
}

} // namespace ECS
