#include "JumpscareSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "ECS/Components/CollisionComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "Collision/AABBCollision.h"
#include "UnoEngine.h"
#include <cmath>

namespace ECS {

static bool damageSoundLoaded = false;

void JumpscareSystem::Update(World& world, float deltaTime) {
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    if (!gameStateEntity.IsValid()) return;

    auto& gameState = world.GetComponent<GameStateComponent>(gameStateEntity);

    // Load damage SE once
    if (!damageSoundLoaded) {
        UnoEngine::GetInstance()->LoadAudio("damageSE", "Resources/Audio/se/damage.mp3");
        damageSoundLoaded = true;
    }

    // Don't process during game over fade or when already game over
    if (gameState.gameOverFading || gameState.isGameOver) return;

    // Decay damage flash
    if (gameState.damageFlashAlpha > 0.0f) {
        gameState.damageFlashAlpha -= deltaTime * 2.5f;
        if (gameState.damageFlashAlpha < 0.0f) gameState.damageFlashAlpha = 0.0f;
    }

    // Decay cooldown
    if (gameState.damageCooldownTimer > 0.0f) {
        gameState.damageCooldownTimer -= deltaTime;
    }

    // Skip during respawn
    auto& respawn = world.GetComponent<RespawnStateComponent>(gameStateEntity);
    if (respawn.state != RespawnStateComponent::State::None) return;

    // Can't take damage during cooldown
    if (gameState.damageCooldownTimer > 0.0f) return;

    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);

    world.ForEach<EnemyTag, TransformComponent, EnemyAIComponent>(
        [&](Entity entity, EnemyTag&, TransformComponent& enemyTransform,
            EnemyAIComponent& ai) {

            if (!ai.isActive) return;

            bool shouldDamage = false;

            // AABB collision check
            auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
            if (collisionManager) {
                auto& playerRenderer = world.GetComponent<MeshRendererComponent>(playerEntity);
                auto& enemyRenderer = world.GetComponent<MeshRendererComponent>(entity);

                if (playerRenderer.object3d && enemyRenderer.object3d) {
                    auto playerCol = collisionManager->FindCollisionObject(playerRenderer.object3d.get());
                    auto enemyCol = collisionManager->FindCollisionObject(enemyRenderer.object3d.get());

                    if (playerCol && enemyCol && playerCol->IsEnabled() && enemyCol->IsEnabled()) {
                        playerCol->Update();
                        enemyCol->Update();

                        constexpr float expandXZ = 0.4f;
                        constexpr float expandY = 0.5f;
                        Collision::AABB enemyBox = enemyCol->GetWorldAABB();
                        enemyBox.min.x -= expandXZ;
                        enemyBox.min.z -= expandXZ;
                        enemyBox.min.y -= expandY;
                        enemyBox.max.x += expandXZ;
                        enemyBox.max.z += expandXZ;
                        enemyBox.max.y += expandY;

                        shouldDamage = Collision::CheckAABBCollision(playerCol->GetWorldAABB(), enemyBox);
                    }
                }
            }

            // Fallback: XZ distance check
            if (!shouldDamage) {
                float dx = enemyTransform.position.x - playerTransform.position.x;
                float dz = enemyTransform.position.z - playerTransform.position.z;
                float distXZ = std::sqrt(dx * dx + dz * dz);
                shouldDamage = distXZ < 2.0f;
            }

            if (shouldDamage) {
                gameState.captureCount++;
                gameState.damageCooldownTimer = 2.0f;
                gameState.damageFlashAlpha = 0.8f;

                UnoEngine::GetInstance()->PlayAudio("damageSE", false, 0.5f);

                if (gameState.captureCount >= gameState.maxCaptures) {
                    gameState.isGameOver = true;
                    gameState.gameOverFading = true;
                    gameState.gameOverFadeTimer = 0.0f;
                    gameState.damageFlashAlpha = 0.0f; // Game over fade takes over
                }
            }
        }
    );
}

} // namespace ECS
