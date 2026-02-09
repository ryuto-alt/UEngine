#include "StealthSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/AudioComponents.h"
#include <cmath>

namespace ECS {

void StealthSystem::Update(World& world, float deltaTime) {
    // Get player position for distance check
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);

    world.ForEach<EnemyTag, TransformComponent, StealthComponent, EnemyAIComponent>(
        [deltaTime, &playerTransform](Entity entity, EnemyTag&, TransformComponent& transform,
                   StealthComponent& stealth, EnemyAIComponent& ai) {

            if (!stealth.stealthEnabled) return;

            if (ai.isChasing) {
                // Chase started: break stealth
                stealth.stealthActive = false;
                stealth.outOfRangeTimer = 0.0f;
            } else {
                if (stealth.stealthActive) {
                    // Already stealthy, stays silent until found
                    return;
                }

                // Distance check for stealth activation
                float dx = playerTransform.position.x - transform.position.x;
                float dy = playerTransform.position.y - transform.position.y;
                float dz = playerTransform.position.z - transform.position.z;
                float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

                if (distance > stealth.stealthAudioRange) {
                    stealth.outOfRangeTimer += deltaTime;
                    if (stealth.outOfRangeTimer >= stealth.stealthActivationTime) {
                        stealth.stealthActive = true;
                    }
                } else {
                    stealth.outOfRangeTimer = 0.0f;
                }
            }
        }
    );
}

} // namespace ECS
