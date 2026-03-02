#include "RespawnSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/GameStateComponents.h"

namespace ECS {

void RespawnSystem::Update(World& world, float deltaTime) {
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    if (!gameStateEntity.IsValid()) return;

    auto& respawn = world.GetComponent<RespawnStateComponent>(gameStateEntity);
    if (respawn.state == RespawnStateComponent::State::None) return;

    respawn.timer += deltaTime;

    switch (respawn.state) {
    case RespawnStateComponent::State::FadeOut:
        respawn.fadeAlpha = respawn.timer / respawn.fadeDuration;
        if (respawn.fadeAlpha >= 1.0f) {
            respawn.fadeAlpha = 1.0f;

            // Reset positions
            Entity playerEntity = world.FindEntityWith<PlayerTag>();
            if (playerEntity.IsValid()) {
                auto& transform = world.GetComponent<TransformComponent>(playerEntity);
                transform.position = respawn.playerInitialPosition;
                auto& jumpscare = world.GetComponent<JumpscareVictimComponent>(playerEntity);
                jumpscare.isInJumpscare = false;
            }

            world.ForEach<EnemyTag, TransformComponent, EnemyAIComponent,
                         EnemyJumpscareComponent, PathfindingComponent>(
                [&respawn](Entity entity, EnemyTag&, TransformComponent& transform,
                          EnemyAIComponent& ai, EnemyJumpscareComponent& jumpscare,
                          PathfindingComponent& pathfinding) {
                    transform.position = respawn.enemyInitialPosition;
                    ai.isChasing = false;
                    ai.wasChasing = false;
                    ai.isSearching = false;
                    pathfinding.currentPath.clear();
                    pathfinding.waypointIndex = 0;
                    jumpscare.isJumpscaring = false;
                    jumpscare.timer = 0.0f;
                }
            );

            // Reset camera
            Camera* camera = world.GetResource<Camera*>();
            if (camera) {
                Vector3 playerPos = respawn.playerInitialPosition;
                camera->SetTranslate({playerPos.x, playerPos.y + 1.5f, playerPos.z - 5.0f});
                camera->Update();
            }

            // Reset lighting
            auto* lightManager = world.GetResource<LightManager*>();
            if (lightManager) {
                lightManager->Initialize();
            }

            auto& gameState = world.GetComponent<GameStateComponent>(gameStateEntity);
            gameState.jumpscareStarted = false;

            respawn.state = RespawnStateComponent::State::Respawning;
            respawn.timer = 0.0f;
        }
        break;

    case RespawnStateComponent::State::Respawning:
        if (respawn.timer >= 0.5f) {
            respawn.state = RespawnStateComponent::State::FadeIn;
            respawn.timer = 0.0f;
        }
        break;

    case RespawnStateComponent::State::FadeIn:
        respawn.fadeAlpha = 1.0f - (respawn.timer / respawn.fadeDuration);
        if (respawn.fadeAlpha <= 0.0f) {
            respawn.fadeAlpha = 0.0f;
            respawn.state = RespawnStateComponent::State::None;
            respawn.timer = 0.0f;
        }
        break;

    default:
        break;
    }
}

} // namespace ECS
