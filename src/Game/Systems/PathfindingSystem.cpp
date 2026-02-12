#include "PathfindingSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "NavMesh/NavMesh.h"
#include "NavMesh/NavMeshHelper.h"
#include <cmath>
#include <random>

namespace ECS {

namespace {

Vector3 GetRandomPatrolPoint(const Vector3& position, NavMesh* navMesh) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    constexpr float PATROL_RADIUS = 50.0f;
    constexpr int MAX_ATTEMPTS = 20;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
        std::uniform_real_distribution<float> radiusDist(PATROL_RADIUS * 0.3f, PATROL_RADIUS);

        float angle = angleDist(gen);
        float distance = radiusDist(gen);

        Vector3 candidate = {
            position.x + distance * std::cos(angle),
            position.y,
            position.z + distance * std::sin(angle)
        };

        if (navMesh && navMesh->IsValid()) {
            float startPos[3] = {position.x, position.y, position.z};
            float endPos[3] = {candidate.x, candidate.y, candidate.z};

            NavMeshPath testPath;
            if (navMesh->FindPath(startPos, endPos, testPath) && testPath.isValid && testPath.GetWaypointCount() > 0) {
                float x, y, z;
                testPath.GetWaypoint(testPath.GetWaypointCount() - 1, x, y, z);
                return {x, y, z};
            }
        }
    }

    // Fallback: small random offset
    std::uniform_real_distribution<float> smallDist(5.0f, 10.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    float angle = angleDist(gen);
    float distance = smallDist(gen);
    return {position.x + distance * std::cos(angle), position.y, position.z + distance * std::sin(angle)};
}

void FindNavMeshPath(const Vector3& from, const Vector3& to, NavMesh* navMesh,
                     std::vector<Vector3>& outPath, int& outWaypointIndex) {
    outPath.clear();
    outWaypointIndex = 0;

    if (!navMesh) return;

    float startPos[3] = {from.x, from.y, from.z};
    float endPos[3] = {to.x, to.y, to.z};

    NavMeshPath path;
    if (navMesh->FindPath(startPos, endPos, path) && path.isValid) {
        for (int i = 0; i < path.GetWaypointCount(); ++i) {
            float x, y, z;
            path.GetWaypoint(i, x, y, z);
            outPath.push_back({x, y, z});
        }
    }
}

} // anonymous namespace

void PathfindingSystem::Update(World& world, float deltaTime) {
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    Vector3 playerPos = {0, 0, 0};
    if (playerEntity.IsValid()) {
        playerPos = world.GetComponent<TransformComponent>(playerEntity).position;
    }

    world.ForEach<EnemyTag, TransformComponent, RotationSmoothingComponent,
                  EnemyAIComponent, VisionComponent, PathfindingComponent>(
        [deltaTime, &playerPos](Entity entity, EnemyTag&, TransformComponent& transform,
                   RotationSmoothingComponent& rot, EnemyAIComponent& ai,
                   VisionComponent& vision, PathfindingComponent& pathfinding) {

            if (!ai.isActive) return;
            if (!pathfinding.navMesh || !pathfinding.navMesh->IsValid()) return;

            pathfinding.updateTimer -= deltaTime;

            if (ai.isChasing) {
                // Chase: frequent path updates (0.1s)
                if (pathfinding.updateTimer <= 0.0f) {
                    FindNavMeshPath(transform.position, playerPos, pathfinding.navMesh,
                                   pathfinding.currentPath, pathfinding.waypointIndex);
                    pathfinding.updateTimer = 0.4f;
                }
            } else if (ai.isSearching) {
                // Search: path to last heard sound position
                if (pathfinding.updateTimer <= 0.0f) {
                    FindNavMeshPath(transform.position, vision.lastSeenPlayerPosition, pathfinding.navMesh,
                                   pathfinding.currentPath, pathfinding.waypointIndex);
                    pathfinding.updateTimer = 0.2f;
                }
            } else {
                // Patrol: wander to random points
                if (pathfinding.currentPath.empty() ||
                    pathfinding.waypointIndex >= static_cast<int>(pathfinding.currentPath.size())) {
                    Vector3 randomPoint = GetRandomPatrolPoint(transform.position, pathfinding.navMesh);
                    FindNavMeshPath(transform.position, randomPoint, pathfinding.navMesh,
                                   pathfinding.currentPath, pathfinding.waypointIndex);
                }

                // Periodic re-pathing during patrol
                if (pathfinding.updateTimer <= 0.0f && !pathfinding.currentPath.empty()) {
                    Vector3 target = pathfinding.currentPath.back();
                    FindNavMeshPath(transform.position, target, pathfinding.navMesh,
                                   pathfinding.currentPath, pathfinding.waypointIndex);
                    pathfinding.updateTimer = pathfinding.updateInterval;
                }
            }

            // Follow path using NavMeshHelper
            if (!pathfinding.currentPath.empty()) {
                float currentMoveSpeed;
                if (ai.isChasing) {
                    currentMoveSpeed = ai.moveSpeed;
                } else if (ai.isSearching) {
                    currentMoveSpeed = ai.searchMoveSpeed;
                } else {
                    currentMoveSpeed = ai.patrolMoveSpeed;
                }

                NavMeshHelper::FollowPath(
                    transform.position,
                    rot.currentRotationY,
                    pathfinding.currentSpeed,
                    pathfinding.currentPath,
                    pathfinding.waypointIndex,
                    currentMoveSpeed,
                    deltaTime,
                    pathfinding.navMesh,
                    &pathfinding.isAtCorner,
                    &pathfinding.cornerSlowdown,
                    ai.isChasing
                );

                // Sync rotation: FollowPath handles its own smoothing,
                // prevent RotationSmoothingSystem from overriding
                rot.targetRotationY = rot.currentRotationY;
                transform.rotation.y = rot.currentRotationY;
            }
        }
    );
}

} // namespace ECS
