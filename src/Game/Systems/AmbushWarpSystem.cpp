#include "AmbushWarpSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/CollectibleComponents.h"
#include "NavMesh/NavMeshHelper.h"
#include "NavMesh/NavMesh.h"
#include <cmath>
#include <random>

namespace ECS {

void AmbushWarpSystem::Update(World& world, float deltaTime) {
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);
    Vector3 playerPos = playerTransform.position;

    // Find nearest uncollected orb to player
    float nearestOrbDist = 1e9f;
    Vector3 nearestOrbPos{};
    bool hasOrb = false;

    world.ForEach<CollectibleTag, TransformComponent, OrbComponent>(
        [&](Entity e, CollectibleTag&, TransformComponent& orbTransform, OrbComponent& orb) {
            if (orb.isCollected) return;
            float dx = orbTransform.position.x - playerPos.x;
            float dz = orbTransform.position.z - playerPos.z;
            float dist = dx * dx + dz * dz;
            if (dist < nearestOrbDist) {
                nearestOrbDist = dist;
                nearestOrbPos = orbTransform.position;
                hasOrb = true;
            }
        }
    );

    if (!hasOrb) return;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

    world.ForEach<EnemyTag, TransformComponent, AmbushWarpComponent, EnemyAIComponent,
                  VisionComponent, PathfindingComponent, RotationSmoothingComponent>(
        [&](Entity entity, EnemyTag&, TransformComponent& transform,
            AmbushWarpComponent& ambush, EnemyAIComponent& ai,
            VisionComponent& vision,
            PathfindingComponent& pathfinding, RotationSmoothingComponent& rotSmoothing) {

            // Cooldown countdown
            if (ambush.warpCooldownTimer > 0.0f) {
                ambush.warpCooldownTimer -= deltaTime;
            }
            // Slowdown countdown
            if (ambush.slowdownTimer > 0.0f) {
                ambush.slowdownTimer -= deltaTime;
            }

            // Reset safety timer only when actually chasing (found the player)
            // Searching (heard footsteps, looking around) does NOT reset -
            // player evading a search still counts toward the ambush timer
            if (ai.isChasing) {
                ambush.safetyTimer = 0.0f;
                return;
            }

            if (!ai.isActive) return;

            ambush.safetyTimer += deltaTime;

            if (ambush.safetyTimer < AmbushWarpComponent::kSafetyThreshold) return;
            if (ambush.warpCooldownTimer > 0.0f) return;

            // Distance check
            float dx = transform.position.x - playerPos.x;
            float dy = transform.position.y - playerPos.y;
            float dz = transform.position.z - playerPos.z;
            float enemyDist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (enemyDist < AmbushWarpComponent::kMinEnemyDistance) return;

            // Random chance per second
            if (chanceDist(rng) > AmbushWarpComponent::kRandomChancePerSec * deltaTime) return;

            // Calculate warp position: player -> nearest orb direction, kWarpDistance from player
            float dirX = nearestOrbPos.x - playerPos.x;
            float dirZ = nearestOrbPos.z - playerPos.z;
            float dirLen = std::sqrt(dirX * dirX + dirZ * dirZ);
            if (dirLen < 0.001f) return;
            dirX /= dirLen;
            dirZ /= dirLen;

            Vector3 warpPos;
            warpPos.x = playerPos.x + dirX * AmbushWarpComponent::kWarpDistance;
            warpPos.y = playerPos.y;
            warpPos.z = playerPos.z + dirZ * AmbushWarpComponent::kWarpDistance;

            // Clamp to NavMesh
            if (pathfinding.navMesh) {
                warpPos = NavMeshHelper::ClampToNavMesh(warpPos, pathfinding.navMesh);
            }

            // Check distance after clamping (don't warp too close)
            float clampDx = warpPos.x - playerPos.x;
            float clampDz = warpPos.z - playerPos.z;
            float clampDist = std::sqrt(clampDx * clampDx + clampDz * clampDz);
            if (clampDist < AmbushWarpComponent::kMinWarpDistance) return;

            // Raycast: ensure line of sight from warp position to player
            if (pathfinding.navMesh && !pathfinding.navMesh->Raycast(warpPos, playerPos)) {
                return; // Wall blocks line of sight - skip
            }

            // Execute warp
            transform.position = warpPos;

            float rotationY = std::atan2(playerPos.x - warpPos.x, playerPos.z - warpPos.z);
            rotSmoothing.currentRotationY = rotationY;
            rotSmoothing.targetRotationY = rotationY;
            transform.rotation.y = rotationY;

            // Trigger chase (wasChasing stays false -> BarkSoundSystem will fire bark)
            ai.isChasing = true;
            vision.lastSeenPlayerPosition = playerPos;
            vision.lostSightTimer = 0.0f;

            // Clear pathfinding
            pathfinding.currentPath.clear();
            pathfinding.waypointIndex = 0;

            // Reset ambush timers
            ambush.safetyTimer = 0.0f;
            ambush.warpCooldownTimer = AmbushWarpComponent::kWarpCooldown;
            ambush.slowdownTimer = AmbushWarpComponent::kSlowdownDuration;
        }
    );
}

} // namespace ECS
