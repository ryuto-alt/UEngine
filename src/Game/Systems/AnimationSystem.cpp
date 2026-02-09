#include "AnimationSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/RenderComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"

namespace ECS {

void AnimationSystem::Update(World& world, float deltaTime) {
    // Player animation: pause when not moving, play when moving
    world.ForEach<PlayerTag, PlayerMovementComponent, AnimatedModelComponent>(
        [deltaTime](Entity entity, PlayerTag&, PlayerMovementComponent& movement,
                   AnimatedModelComponent& anim) {
            if (!anim.animatedModel) return;

            // Blend timer
            if (anim.isBlending) {
                anim.blendTimer += deltaTime;
                if (anim.blendTimer >= anim.blendDuration) {
                    anim.isBlending = false;
                    anim.blendTimer = 0.0f;
                }
            }

            // Play/pause based on movement
            if (movement.isMoving) {
                if (anim.animationPaused) {
                    anim.animationPaused = false;
                    anim.animatedModel->PlayAnimation();
                }
            } else {
                if (!anim.animationPaused) {
                    anim.animationPaused = true;
                    anim.animatedModel->PauseAnimation();
                }
            }

            // Advance animation
            if (!anim.animationPaused) {
                anim.animatedModel->Update(deltaTime * anim.animationSpeed);
            } else {
                anim.animatedModel->Update(0.0f);
            }
        }
    );

    // Enemy animation
    world.ForEach<EnemyTag, EnemyAIComponent, AnimatedModelComponent>(
        [deltaTime](Entity entity, EnemyTag&, EnemyAIComponent& ai,
                   AnimatedModelComponent& anim) {
            if (!anim.animatedModel) return;

            // Blend timer
            if (anim.isBlending) {
                anim.blendTimer += deltaTime;
                if (anim.blendTimer >= anim.blendDuration) {
                    anim.isBlending = false;
                    anim.blendTimer = 0.0f;
                }
            }

            // Normal animation update
            if (!anim.animationPaused) {
                anim.animatedModel->Update(deltaTime * anim.animationSpeed);
            } else {
                anim.animatedModel->Update(0.0f);
            }
        }
    );
}

} // namespace ECS
