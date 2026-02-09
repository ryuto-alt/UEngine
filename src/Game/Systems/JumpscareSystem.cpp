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

void JumpscareSystem::Update(World& world, float deltaTime) {
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    if (!gameStateEntity.IsValid()) return;

    auto& gameState = world.GetComponent<GameStateComponent>(gameStateEntity);
    if (gameState.isGameOver) return;

    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);
    auto& playerJumpscare = world.GetComponent<JumpscareVictimComponent>(playerEntity);

    Camera* camera = world.GetResource<Camera*>();
    auto* lightManager = world.GetResource<LightManager*>();

    world.ForEach<EnemyTag, TransformComponent, EnemyJumpscareComponent,
                  EnemyAIComponent, AnimatedModelComponent>(
        [&](Entity entity, EnemyTag&, TransformComponent& enemyTransform,
            EnemyJumpscareComponent& jumpscare, EnemyAIComponent& ai,
            AnimatedModelComponent& anim) {

            // Trigger check: AABB overlap or distance fallback
            if (!jumpscare.isJumpscaring && !gameState.jumpscareStarted) {
                bool shouldTrigger = false;

                auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
                if (collisionManager) {
                    // Try AABB check with expanded enemy box
                    auto& playerRenderer = world.GetComponent<MeshRendererComponent>(playerEntity);
                    auto& enemyRenderer = world.GetComponent<MeshRendererComponent>(entity);

                    if (playerRenderer.object3d && enemyRenderer.object3d) {
                        auto playerCol = collisionManager->FindCollisionObject(playerRenderer.object3d.get());
                        auto enemyCol = collisionManager->FindCollisionObject(enemyRenderer.object3d.get());

                        if (playerCol && enemyCol && playerCol->IsEnabled() && enemyCol->IsEnabled()) {
                            playerCol->Update();
                            enemyCol->Update();

                            constexpr float expand = 0.3f;
                            Collision::AABB enemyBox = enemyCol->GetWorldAABB();
                            enemyBox.min.x -= expand; enemyBox.min.z -= expand;
                            enemyBox.max.x += expand; enemyBox.max.z += expand;

                            shouldTrigger = Collision::CheckAABBCollision(playerCol->GetWorldAABB(), enemyBox);
                        }
                    }
                }

                // Fallback: XZ distance check
                if (!shouldTrigger) {
                    float dx = enemyTransform.position.x - playerTransform.position.x;
                    float dz = enemyTransform.position.z - playerTransform.position.z;
                    float distXZ = std::sqrt(dx * dx + dz * dz);
                    shouldTrigger = distXZ < 1.0f;
                }

                if (shouldTrigger) {
                    jumpscare.isJumpscaring = true;
                    jumpscare.timer = 0.0f;
                    playerJumpscare.isInJumpscare = true;
                    gameState.jumpscareStarted = true;

                    if (anim.animatedModel) {
                        anim.animatedModel->ChangeAnimation("Jumpscare");
                        anim.animatedModel->PlayAnimation();
                    }
                }
            }

            // Jumpscare in progress: update camera to face enemy
            if (jumpscare.isJumpscaring && gameState.jumpscareStarted && camera) {
                jumpscare.timer += deltaTime;

                // Camera looks at enemy head
                Vector3 enemyHeadPos = enemyTransform.position;
                enemyHeadPos.y += 2.0f; // Approximate head height

                if (anim.animatedModel) {
                    const Skeleton& skeleton = anim.animatedModel->GetSkeleton();
                    auto headIt = skeleton.jointMap.find("mixamorig:Head");
                    if (headIt != skeleton.jointMap.end()) {
                        const Joint& headJoint = skeleton.joints[headIt->second];
                        constexpr float modelScale = 0.05f;
                        enemyHeadPos = {
                            enemyTransform.position.x + headJoint.skeletonSpaceMatrix.m[3][0] * modelScale,
                            enemyTransform.position.y + headJoint.skeletonSpaceMatrix.m[3][1] * modelScale,
                            enemyTransform.position.z + headJoint.skeletonSpaceMatrix.m[3][2] * modelScale
                        };
                    }
                }

                Vector3 playerToHead = {
                    enemyHeadPos.x - playerTransform.position.x,
                    enemyHeadPos.y - playerTransform.position.y,
                    enemyHeadPos.z - playerTransform.position.z
                };
                float length = std::sqrt(playerToHead.x * playerToHead.x +
                                         playerToHead.y * playerToHead.y +
                                         playerToHead.z * playerToHead.z);
                if (length > 0.0f) {
                    playerToHead.x /= length;
                    playerToHead.y /= length;
                    playerToHead.z /= length;
                }

                // Slight left rotation offset (-30 degrees)
                constexpr float angleOffset = -30.0f * 3.14159f / 180.0f;
                float cosA = std::cos(angleOffset);
                float sinA = std::sin(angleOffset);
                Vector3 adjustedDir = {
                    playerToHead.x * cosA - playerToHead.z * sinA,
                    playerToHead.y,
                    playerToHead.x * sinA + playerToHead.z * cosA
                };

                constexpr float camDist = 3.0f;
                constexpr float heightOffset = -0.4f;
                Vector3 camPos = {
                    enemyHeadPos.x - adjustedDir.x * camDist,
                    enemyHeadPos.y + heightOffset,
                    enemyHeadPos.z - adjustedDir.z * camDist
                };
                camera->SetTranslate(camPos);

                Vector3 camToHead = {
                    enemyHeadPos.x - camPos.x,
                    enemyHeadPos.y - camPos.y,
                    enemyHeadPos.z - camPos.z
                };
                float hDist = std::sqrt(camToHead.x * camToHead.x + camToHead.z * camToHead.z);
                float rotY = std::atan2(camToHead.x, camToHead.z);
                float rotX = -std::atan2(camToHead.y, hDist);
                camera->SetRotate({rotX, rotY, 0.0f});
                camera->Update();

                // Spotlight on face
                if (lightManager) {
                    SpotLight jumpscareLight;
                    jumpscareLight.position = camPos;
                    jumpscareLight.direction = camToHead;
                    jumpscareLight.color = {1.0f, 1.0f, 1.0f, 1.0f};
                    jumpscareLight.intensity = 15.0f;
                    jumpscareLight.innerCone = std::cos(45.0f * 3.14159f / 180.0f);
                    jumpscareLight.outerCone = std::cos(60.0f * 3.14159f / 180.0f);
                    jumpscareLight.attenuation = {1.0f, 0.1f, 0.01f};
                    lightManager->SetJumpscareLight(jumpscareLight);
                }
            }

            // Jumpscare finished
            if (jumpscare.isJumpscaring && jumpscare.timer >= jumpscare.duration) {
                gameState.captureCount++;

                if (gameState.captureCount >= gameState.maxCaptures) {
                    gameState.isGameOver = true;

                    // Launch jumpscare process and exit
                    char exePath[MAX_PATH];
                    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

                    STARTUPINFOA si = {};
                    si.cb = sizeof(si);
                    si.dwFlags = STARTF_USESHOWWINDOW;
                    si.wShowWindow = SW_HIDE;
                    PROCESS_INFORMATION pi = {};

                    char cmdLine[MAX_PATH + 20];
                    sprintf_s(cmdLine, "\"%s\" --jumpscare", exePath);

                    if (CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE,
                                       DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                    ExitProcess(0);
                } else {
                    // Start respawn
                    if (gameStateEntity.IsValid()) {
                        auto& respawn = world.GetComponent<RespawnStateComponent>(gameStateEntity);
                        respawn.state = RespawnStateComponent::State::FadeOut;
                        respawn.timer = 0.0f;
                        respawn.fadeAlpha = 0.0f;
                    }
                }

                jumpscare.isJumpscaring = false;
                jumpscare.timer = 0.0f;
                gameState.jumpscareStarted = false;
            }
        }
    );
}

} // namespace ECS
