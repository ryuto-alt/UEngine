#include "SoundDetectionSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "UnoEngine.h"
#include <cmath>

namespace ECS {

void SoundDetectionSystem::Update(World& world, float deltaTime) {
    // Find player footstep data
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);
    auto& playerFootstep = world.GetComponent<PlayerFootstepComponent>(playerEntity);

    float currentTime = static_cast<float>(UnoEngine::GetInstance()->GetTotalTime());
    float timeSinceFootstep = currentTime - playerFootstep.lastFootstepTime;
    bool hasRecentFootstep = timeSinceFootstep <= 0.5f;

    world.ForEach<EnemyTag, TransformComponent, SoundDetectionComponent, EnemyAIComponent,
                  VisionComponent, PathfindingComponent>(
        [&](Entity entity, EnemyTag&, TransformComponent& transform,
            SoundDetectionComponent& sound, EnemyAIComponent& ai,
            VisionComponent& vision, PathfindingComponent& pathfinding) {

            if (!ai.isActive) return;
            if (!hasRecentFootstep) return;

            Vector3 footstepPos = playerFootstep.lastFootstepPosition;
            float dx = footstepPos.x - transform.position.x;
            float dz = footstepPos.z - transform.position.z;
            float distance = std::sqrt(dx * dx + dz * dz);

            if (distance <= sound.soundDetectionRange) {
                sound.lastHeardPosition = footstepPos;
                sound.lastSoundTime = currentTime;

                // If not already chasing, enter or continue search mode
                if (!ai.isChasing && !ai.isSearching) {
                    ai.isSearching = true;
                    ai.searchTimer = 0.0f;
                    vision.lastSeenPlayerPosition = footstepPos;
                    pathfinding.updateTimer = 0.0f;
                } else if (!ai.isChasing) {
                    // New sound heard during search: reset timer (stay persistent)
                    ai.searchTimer = 0.0f;

                    // Update sound target if significantly different
                    float sdx = footstepPos.x - vision.lastSeenPlayerPosition.x;
                    float sdz = footstepPos.z - vision.lastSeenPlayerPosition.z;
                    float soundMoveDist = std::sqrt(sdx * sdx + sdz * sdz);

                    if (soundMoveDist > 2.0f) {
                        vision.lastSeenPlayerPosition = footstepPos;
                        pathfinding.updateTimer = 0.0f;
                    }
                }
            }
        }
    );
}

} // namespace ECS
