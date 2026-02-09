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

            if (!ai.isActive) return;

            if (stuck.isRecovering) {
                // Recovery: clear path and recalculate
                pathfinding.currentPath.clear();
                pathfinding.waypointIndex = 0;

                if (ai.isChasing && pathfinding.navMesh) {
                    pathfinding.updateTimer = 0.0f;
                }

                stuck.isRecovering = false;
                stuck.recoveryAttempts = 0;
                stuck.isTracking = false;
                return;
            }

            // Start tracking from current position
            if (!stuck.isTracking) {
                stuck.checkOrigin = transform.position;
                stuck.isTracking = true;
                stuck.timer = 0.0f;
            }

            stuck.timer += deltaTime;

            // Check cumulative distance over the detection period
            if (stuck.timer >= stuck.detectionTime) {
                float dx = transform.position.x - stuck.checkOrigin.x;
                float dz = transform.position.z - stuck.checkOrigin.z;
                float totalDist = std::sqrt(dx * dx + dz * dz);

                if (totalDist < stuck.distanceThreshold) {
                    // Truly stuck: hasn't moved enough over the entire period
                    stuck.isRecovering = true;
                    pathfinding.currentPath.clear();
                    pathfinding.waypointIndex = 0;
                }

                // Reset tracking for next period
                stuck.checkOrigin = transform.position;
                stuck.timer = 0.0f;
            }

            prevPos.previousPosition = transform.position;
        }
    );
}

} // namespace ECS
