#include "CameraSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/CameraComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "UnoEngine.h"
#include "GameObject/FPSCamera.h"

namespace ECS {

void CameraSystem::Update(World& world, float deltaTime) {
    // Skip camera update during jumpscare
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    if (gameStateEntity.IsValid()) {
        auto& gameState = world.GetComponent<GameStateComponent>(gameStateEntity);
        if (gameState.jumpscareStarted) return;
    }

    UnoEngine* engine = UnoEngine::GetInstance();
    Camera* camera = world.GetResource<Camera*>();
    if (!camera) return;

    world.ForEach<PlayerTag, TransformComponent, PlayerMovementComponent,
                  FPSCameraComponent, CameraFollowComponent>(
        [engine, camera, deltaTime](Entity entity, PlayerTag&, TransformComponent& transform,
                   PlayerMovementComponent& movement, FPSCameraComponent& fpsCam,
                   CameraFollowComponent& follow) {

            if (fpsCam.fpsCamera && fpsCam.isFPSMode) {
                // FPS mode: FPSCamera handles rotation
                fpsCam.fpsCamera->UpdateCameraRotation(camera, engine);
                fpsCam.fpsCamera->UpdateCamera(camera, transform.position, nullptr);
                camera->Update();
            } else {
                // Third-person orbit mode
                engine->UpdCamMouse();
                engine->UpdCamStick();

                // Smooth position interpolation for camera target
                movement.smoothedPosition.x += (transform.position.x - movement.smoothedPosition.x) * follow.smoothingFactor;
                movement.smoothedPosition.y += (transform.position.y - movement.smoothedPosition.y) * follow.smoothingFactor;
                movement.smoothedPosition.z += (transform.position.z - movement.smoothedPosition.z) * follow.smoothingFactor;

                camera->SetOrbitTarget(movement.smoothedPosition);
                camera->SetOrbitDistance(follow.orbitDistance);
                camera->SetOrbitHeight(follow.orbitHeight);
                camera->UpdateOrbitCamera();
                camera->Update();
            }
        }
    );
}

} // namespace ECS
