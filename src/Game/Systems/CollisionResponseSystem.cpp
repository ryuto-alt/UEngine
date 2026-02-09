#include "CollisionResponseSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "ECS/Components/CollisionComponents.h"
#include "Collision/AABBCollision.h"
#include <cmath>
#include <algorithm>

namespace ECS {

void CollisionResponseSystem::Update(World& world, float deltaTime) {
    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (!collisionManager) return;

    world.ForEach<PlayerTag, TransformComponent, MeshRendererComponent>(
        [collisionManager](Entity entity, PlayerTag&, TransformComponent& transform,
                          MeshRendererComponent& renderer) {
            if (!renderer.object3d) return;

            auto playerColObj = collisionManager->FindCollisionObject(renderer.object3d.get());
            if (!playerColObj || !playerColObj->IsEnabled()) return;

            constexpr int maxIterations = 3;

            for (int iteration = 0; iteration < maxIterations; ++iteration) {
                renderer.object3d->SetPosition(transform.position);
                renderer.object3d->Update();
                playerColObj->Update();

                const Collision::AABB& playerAABB = playerColObj->GetWorldAABB();

                bool hasCollision = false;
                Vector3 pushBack = {0.0f, 0.0f, 0.0f};

                const auto& allObjects = collisionManager->GetCollisionObjects();
                for (const auto& colObj : allObjects) {
                    if (colObj->GetObject() == renderer.object3d.get()) continue;
                    if (!colObj->IsEnabled()) continue;

                    const Collision::AABB& otherAABB = colObj->GetWorldAABB();

                    if (Collision::CheckAABBCollision(playerAABB, otherAABB)) {
                        hasCollision = true;

                        float overlapX = (std::min)(playerAABB.max.x - otherAABB.min.x,
                                                  otherAABB.max.x - playerAABB.min.x);
                        float overlapY = (std::min)(playerAABB.max.y - otherAABB.min.y,
                                                  otherAABB.max.y - playerAABB.min.y);
                        float overlapZ = (std::min)(playerAABB.max.z - otherAABB.min.z,
                                                  otherAABB.max.z - playerAABB.min.z);

                        constexpr float margin = 0.005f;

                        if (overlapX <= overlapY && overlapX <= overlapZ) {
                            if (playerAABB.GetCenter().x < otherAABB.GetCenter().x)
                                pushBack.x = -(overlapX + margin);
                            else
                                pushBack.x = (overlapX + margin);
                        } else if (overlapY <= overlapX && overlapY <= overlapZ) {
                            if (playerAABB.GetCenter().y < otherAABB.GetCenter().y)
                                pushBack.y = -(overlapY + margin);
                            else
                                pushBack.y = (overlapY + margin);
                        } else {
                            if (playerAABB.GetCenter().z < otherAABB.GetCenter().z)
                                pushBack.z = -(overlapZ + margin);
                            else
                                pushBack.z = (overlapZ + margin);
                        }
                        break;
                    }
                }

                if (!hasCollision) break;

                transform.position.x += pushBack.x;
                transform.position.y += pushBack.y;
                transform.position.z += pushBack.z;
            }

            renderer.object3d->SetPosition(transform.position);
            renderer.object3d->Update();
        }
    );
}

} // namespace ECS
