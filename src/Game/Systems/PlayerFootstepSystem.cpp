#include "PlayerFootstepSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "UnoEngine.h"

namespace ECS {

void PlayerFootstepSystem::Update(World& world, float deltaTime) {
    world.ForEach<PlayerTag, TransformComponent, PlayerMovementComponent,
                  PlayerFootstepComponent, GravityComponent>(
        [deltaTime](Entity entity, PlayerTag&, TransformComponent& transform,
                   PlayerMovementComponent& movement, PlayerFootstepComponent& footstep,
                   GravityComponent& gravity) {

            if (movement.isMoving && gravity.isGrounded) {
                footstep.footstepTimer += deltaTime;

                float currentInterval = movement.isRunning
                    ? footstep.footstepInterval * 0.6f
                    : footstep.footstepInterval;

                if (footstep.footstepTimer >= currentInterval) {
                    UnoEngine* engine = UnoEngine::GetInstance();
                    footstep.lastFootstepTime = static_cast<float>(engine->GetTotalTime());
                    footstep.lastFootstepPosition = transform.position;
                    footstep.footstepTimer = 0.0f;
                }
            } else {
                footstep.footstepTimer = 0.0f;
            }
        }
    );
}

} // namespace ECS
