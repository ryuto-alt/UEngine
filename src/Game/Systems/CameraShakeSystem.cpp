#include "CameraShakeSystem.h"
#include "ECS/World.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/CameraComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "UnoEngine.h"
#include "GameObject/FPSCamera.h"

namespace ECS {

void CameraShakeSystem::Update(World& world, float deltaTime) {
    // Skip during jumpscare
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    if (gameStateEntity.IsValid()) {
        auto& gameState = world.GetComponent<GameStateComponent>(gameStateEntity);
        if (gameState.jumpscareStarted) return;
    }

    UnoEngine* engine = UnoEngine::GetInstance();

    world.ForEach<PlayerTag, PlayerMovementComponent, FPSCameraComponent, CameraShakeComponent>(
        [engine, deltaTime](Entity entity, PlayerTag&, PlayerMovementComponent& movement,
                           FPSCameraComponent& fpsCam, CameraShakeComponent& shake) {

            if (!fpsCam.fpsCamera || !fpsCam.isFPSMode) return;

            fpsCam.fpsCamera->UpdateCameraShake(
                movement.isMoving, movement.isRunning,
                deltaTime, engine
            );
        }
    );
}

} // namespace ECS
