#include "VisionSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "NavMesh/NavMesh.h"
#include <cmath>

namespace ECS {

namespace {

// NavMesh raycast with side-ray fallback for corner accuracy
bool RaycastWithFallback(NavMesh* navMesh, const Vector3& from, const Vector3& to, const Vector3& toPlayer) {
    if (navMesh->Raycast(from, to)) return true;

    // Side-offset retry for corner edge cases
    Vector3 offsetRight = {toPlayer.z, 0.0f, -toPlayer.x};
    float len = std::sqrt(offsetRight.x * offsetRight.x + offsetRight.z * offsetRight.z);
    if (len > 0.01f) {
        float invLen = 0.5f / len;
        offsetRight.x *= invLen;
        offsetRight.z *= invLen;

        Vector3 eyeL = {from.x - offsetRight.x, from.y, from.z - offsetRight.z};
        Vector3 eyeR = {from.x + offsetRight.x, from.y, from.z + offsetRight.z};
        return navMesh->Raycast(eyeL, to) || navMesh->Raycast(eyeR, to);
    }
    return false;
}

} // anonymous namespace

void VisionSystem::Update(World& world, float deltaTime) {
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);
    Vector3 playerPos = playerTransform.position;

    world.ForEach<EnemyTag, TransformComponent, RotationSmoothingComponent,
                  VisionComponent, EnemyAIComponent, PathfindingComponent, StealthComponent>(
        [&playerPos, deltaTime](Entity entity, EnemyTag&, TransformComponent& transform,
                   RotationSmoothingComponent& rot, VisionComponent& vision,
                   EnemyAIComponent& ai, PathfindingComponent& pathfinding,
                   StealthComponent& stealth) {

            if (!ai.isActive) return;

            Vector3 toPlayer = {
                playerPos.x - transform.position.x,
                0.0f,
                playerPos.z - transform.position.z
            };
            float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

            bool playerVisible = false;

            // Close proximity: within 3m always detect (no raycast needed at this range)
            if (distanceToPlayer <= 3.0f) {
                playerVisible = true;
            }

            // Proximity detection: within proximityDetectionDistance regardless of direction
            if (!playerVisible && distanceToPlayer <= vision.proximityDetectionDistance) {
                if (pathfinding.navMesh && pathfinding.navMesh->IsValid()) {
                    Vector3 enemyEye = {transform.position.x, transform.position.y + 1.5f, transform.position.z};
                    Vector3 playerEye = {playerPos.x, playerPos.y + 1.5f, playerPos.z};
                    playerVisible = RaycastWithFallback(pathfinding.navMesh, enemyEye, playerEye, toPlayer);
                } else {
                    playerVisible = true;
                }
            }

            if (!playerVisible) {
                // Vision cone check
                float detectionDist = (stealth.stealthEnabled && stealth.stealthActive)
                    ? vision.stealthDetectionDistance
                    : vision.detectionDistance;

                if (distanceToPlayer <= detectionDist && distanceToPlayer > 0.01f) {
                    float invLen = 1.0f / distanceToPlayer;
                    Vector3 toPlayerNorm = {toPlayer.x * invLen, 0.0f, toPlayer.z * invLen};

                    // Facing direction (body rotation)
                    Vector3 forward = {
                        std::sin(rot.currentRotationY),
                        0.0f,
                        std::cos(rot.currentRotationY)
                    };

                    float dot = toPlayerNorm.x * forward.x + toPlayerNorm.z * forward.z;
                    dot = std::clamp(dot, -1.0f, 1.0f);
                    float angleDeg = std::acos(dot) * (180.0f / 3.14159f);

                    // Widen vision cone at corners (rotation lags behind movement)
                    float effectiveAngle = vision.visionAngle;
                    if (pathfinding.isAtCorner) {
                        effectiveAngle = 130.0f;
                    }

                    // Also check movement direction: if enemy is actively moving,
                    // use path direction as secondary vision check
                    bool inVisionCone = (angleDeg <= effectiveAngle);

                    if (!inVisionCone && pathfinding.currentSpeed > 0.5f &&
                        pathfinding.waypointIndex < static_cast<int>(pathfinding.currentPath.size())) {
                        const Vector3& wp = pathfinding.currentPath[pathfinding.waypointIndex];
                        Vector3 moveDir = {wp.x - transform.position.x, 0.0f, wp.z - transform.position.z};
                        float moveDirLen = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
                        if (moveDirLen > 0.01f) {
                            moveDir.x /= moveDirLen;
                            moveDir.z /= moveDirLen;
                            float moveDot = toPlayerNorm.x * moveDir.x + toPlayerNorm.z * moveDir.z;
                            moveDot = std::clamp(moveDot, -1.0f, 1.0f);
                            float moveAngleDeg = std::acos(moveDot) * (180.0f / 3.14159f);
                            inVisionCone = (moveAngleDeg <= vision.visionAngle);
                        }
                    }

                    if (inVisionCone) {
                        // Wall check via NavMesh raycast
                        if (pathfinding.navMesh && pathfinding.navMesh->IsValid()) {
                            Vector3 enemyEye = {transform.position.x, transform.position.y + 1.5f, transform.position.z};
                            Vector3 playerEye = {playerPos.x, playerPos.y + 1.5f, playerPos.z};
                            playerVisible = RaycastWithFallback(pathfinding.navMesh, enemyEye, playerEye, toPlayer);
                        } else {
                            playerVisible = true;
                        }
                    }
                }
            }

            // Update AI state based on vision
            if (playerVisible) {
                vision.lastSeenPlayerPosition = playerPos;
                vision.lostSightTimer = 0.0f;

                if (!ai.isChasing) {
                    ai.isChasing = true;
                    ai.isSearching = false;
                }
            } else if (ai.isChasing) {
                vision.lostSightTimer += deltaTime;
            }

            // Chase persistence: keep chasing within grace period and release distance
            bool shouldChase = playerVisible ||
                (ai.isChasing && vision.lostSightTimer < vision.lostSightGracePeriod
                 && distanceToPlayer <= vision.chaseReleaseDistance);

            if (!shouldChase && ai.isChasing) {
                ai.isChasing = false;
                // Transition chase → search at last known position
                ai.isSearching = true;
                ai.searchTimer = 0.0f;
                pathfinding.currentPath.clear();
                pathfinding.waypointIndex = 0;
                pathfinding.updateTimer = 0.0f;
                vision.lostSightTimer = 0.0f;
            }

            // Search timeout: give up after maxSearchTime with no new sounds
            if (ai.isSearching && !ai.isChasing) {
                ai.searchTimer += deltaTime;
                if (ai.searchTimer >= ai.maxSearchTime) {
                    ai.isSearching = false;
                    ai.searchTimer = 0.0f;
                }
            }
        }
    );
}

} // namespace ECS
