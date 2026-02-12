#include "OrbCollectionSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/CollectibleComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "UnoEngine.h"
#include <cmath>

namespace ECS {

void OrbCollectionSystem::Update(World& world, float deltaTime) {
    // Find player position
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);
    constexpr float playerRadius = 0.5f;

    // Find game state entity
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();

    int collectedThisFrame = 0;

    world.ForEach<TransformComponent, OrbComponent, FloatingAnimationComponent>(
        [&](Entity entity, TransformComponent& orbTransform, OrbComponent& orb,
            FloatingAnimationComponent& floatAnim) {
            if (orb.isCollected) return;

            // Sphere collision check
            float dx = orbTransform.position.x - playerTransform.position.x;
            float dy = orbTransform.position.y - playerTransform.position.y;
            float dz = orbTransform.position.z - playerTransform.position.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            float totalRadius = orb.collisionRadius + playerRadius;
            if (distSq < totalRadius * totalRadius) {
                orb.isCollected = true;
                collectedThisFrame++;

                // Play collection sound
                UnoEngine* engine = UnoEngine::GetInstance();
                engine->PlayAudio("orbGet", false, 0.7f);
            }
        }
    );

    if (collectedThisFrame > 0 && gameStateEntity.IsValid()) {
        // Count remaining orbs
        int remaining = 0;
        world.ForEach<OrbComponent>(
            [&remaining](Entity entity, OrbComponent& orb) {
                if (!orb.isCollected) remaining++;
            }
        );

        // All orbs collected — trigger ending sequence
        if (remaining == 0) {
            auto& gameState = world.GetComponent<GameStateComponent>(gameStateEntity);
            if (!gameState.allOrbsCollected) {
                gameState.allOrbsCollected = true;

                // Freeze player
                world.ForEach<JumpscareVictimComponent, PlayerMovementComponent>(
                    [](Entity e, JumpscareVictimComponent& victim, PlayerMovementComponent& movement) {
                        victim.isInJumpscare = true;
                        movement.isMoving = false;
                    }
                );

                // Stop enemy AI and clear path
                world.ForEach<EnemyAIComponent, PathfindingComponent>(
                    [](Entity e, EnemyAIComponent& ai, PathfindingComponent& path) {
                        ai.isActive = false;
                        path.currentPath.clear();
                    }
                );
            }
        }

        // Trigger stealth mode at threshold
        if (remaining <= 25) {
            world.ForEach<EnemyTag, StealthComponent>(
                [](Entity entity, EnemyTag&, StealthComponent& stealth) {
                    if (!stealth.stealthEnabled) {
                        stealth.stealthEnabled = true;
                    }
                }
            );
        }
    }
}

} // namespace ECS
