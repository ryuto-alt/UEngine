#include "StuckDetectionSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include <cmath>

namespace ECS {

void StuckDetectionSystem::Update(World& world, float deltaTime) {
    world.ForEach<EnemyTag, TransformComponent, PreviousPositionComponent,
                  StuckDetectionComponent, PathfindingComponent, EnemyAIComponent>(
        [deltaTime, &world](Entity entity, EnemyTag&, TransformComponent& transform,
                  PreviousPositionComponent& prevPos, StuckDetectionComponent& stuck,
                  PathfindingComponent& pathfinding, EnemyAIComponent& ai) {

            if (stuck.isRecovering) {
                // Recovery: clear path and recalculate
                pathfinding.currentPath.clear();
                pathfinding.waypointIndex = 0;

                if (ai.isChasing && pathfinding.navMesh) {
                    // Will be recalculated by PathfindingSystem next frame
                    pathfinding.updateTimer = 0.0f;
                }

                stuck.isRecovering = false;
                stuck.recoveryAttempts = 0;
                return;
            }

            // Calculate movement since last frame
            float dx = transform.position.x - prevPos.previousPosition.x;
            float dz = transform.position.z - prevPos.previousPosition.z;
            float movementDist = std::sqrt(dx * dx + dz * dz);

            if (movementDist < stuck.distanceThreshold) {
                stuck.timer += deltaTime;

                if (stuck.timer >= stuck.detectionTime) {
                    stuck.isRecovering = true;
                    stuck.timer = 0.0f;
                    pathfinding.currentPath.clear();
                    pathfinding.waypointIndex = 0;
                }
            } else {
                stuck.timer = 0.0f;
            }

            prevPos.previousPosition = transform.position;
        }
    );
}

} // namespace ECS
