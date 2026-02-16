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
#include <fstream>
#include <format>
#include <chrono>

namespace ECS {

void JumpscareSystem::Update(World& world, float deltaTime) {
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    if (!gameStateEntity.IsValid()) return;

    auto& gameState = world.GetComponent<GameStateComponent>(gameStateEntity);
    if (gameState.isGameOver) return;

    // Skip during respawn to prevent re-triggering jumpscare
    auto& respawn = world.GetComponent<RespawnStateComponent>(gameStateEntity);
    if (respawn.state != RespawnStateComponent::State::None) return;

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

            if (!ai.isActive) return;

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

                            constexpr float expandXZ = 1.2f; // Wider horizontal trigger
                            constexpr float expandY = 0.8f;  // Vertical trigger
                            Collision::AABB enemyBox = enemyCol->GetWorldAABB();
                            enemyBox.min.x -= expandXZ;
                            enemyBox.min.z -= expandXZ;
                            enemyBox.min.y -= expandY;
                            enemyBox.max.x += expandXZ;
                            enemyBox.max.z += expandXZ;
                            enemyBox.max.y += expandY;

                            shouldTrigger = Collision::CheckAABBCollision(playerCol->GetWorldAABB(), enemyBox);
                        }
                    }
                }

                // Fallback: XZ distance check (very generous)
                if (!shouldTrigger) {
                    float dx = enemyTransform.position.x - playerTransform.position.x;
                    float dz = enemyTransform.position.z - playerTransform.position.z;
                    float distXZ = std::sqrt(dx * dx + dz * dz);
                    shouldTrigger = distXZ < 5.5f; // Very wide trigger zone (enemy scale is 0.05)

                    // Debug output to file
                    if (distXZ < 6.0f) {
                        std::ofstream debugFile("C:\\Users\\Unoryuto\\Documents\\UI\\jumpscare_debug.txt",
                                               std::ios::app);
                        if (debugFile.is_open()) {
                            auto now = std::chrono::system_clock::now();
                            auto time = std::chrono::system_clock::to_time_t(now);
                            debugFile << std::format("[{}] Distance: {:.2f}m (trigger at <5.5m) | "
                                                    "Player({:.2f}, {:.2f}, {:.2f}) | "
                                                    "Enemy({:.2f}, {:.2f}, {:.2f}) | "
                                                    "Triggered: {}\n",
                                                    time, distXZ,
                                                    playerTransform.position.x,
                                                    playerTransform.position.y,
                                                    playerTransform.position.z,
                                                    enemyTransform.position.x,
                                                    enemyTransform.position.y,
                                                    enemyTransform.position.z,
                                                    shouldTrigger ? "YES" : "NO");
                        }
                    }
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

                // Get enemy head position
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

                // Calculate camera position directly in front of enemy face
                // Use enemy's forward direction (based on rotation.y)
                float enemyForwardX = std::sin(enemyTransform.rotation.y);
                float enemyForwardZ = std::cos(enemyTransform.rotation.y);

                constexpr float camDist = 2.5f;
                constexpr float heightOffset = -0.2f;
                constexpr float sideOffset = 0.3f; // Slight side offset for better angle

                Vector3 camPos = {
                    enemyHeadPos.x + enemyForwardX * camDist - enemyForwardZ * sideOffset,
                    enemyHeadPos.y + heightOffset,
                    enemyHeadPos.z + enemyForwardZ * camDist + enemyForwardX * sideOffset
                };
                camera->SetTranslate(camPos);

                // Make camera look at enemy head
                Vector3 camToHead = {
                    enemyHeadPos.x - camPos.x,
                    enemyHeadPos.y - camPos.y,
                    enemyHeadPos.z - camPos.z
                };
                float length = std::sqrt(camToHead.x * camToHead.x +
                                         camToHead.y * camToHead.y +
                                         camToHead.z * camToHead.z);
                if (length > 0.0f) {
                    camToHead.x /= length;
                    camToHead.y /= length;
                    camToHead.z /= length;
                }

                float hDist = std::sqrt(camToHead.x * camToHead.x + camToHead.z * camToHead.z);
                float rotY = std::atan2(camToHead.x, camToHead.z);
                float rotX = -std::atan2(camToHead.y, hDist);
                camera->SetRotate({rotX, rotY, 0.0f});
                camera->Update();

                // Strong spotlight from camera position
                if (lightManager) {
                    SpotLight jumpscareLight;
                    jumpscareLight.position = camPos;
                    jumpscareLight.direction = camToHead;
                    jumpscareLight.color = {1.0f, 0.95f, 0.9f, 1.0f}; // Slightly warm
                    jumpscareLight.intensity = 40.0f; // Increased from 15.0
                    jumpscareLight.innerCone = std::cos(35.0f * 3.14159f / 180.0f);
                    jumpscareLight.outerCone = std::cos(50.0f * 3.14159f / 180.0f);
                    jumpscareLight.attenuation = {1.0f, 0.05f, 0.005f};
                    lightManager->SetJumpscareLight(jumpscareLight);
                }
            }

            // Jumpscare finished
            if (jumpscare.duration <= 0.0f) jumpscare.duration = 2.0f; // Safety fallback
            if (jumpscare.isJumpscaring && jumpscare.timer >= jumpscare.duration) {
                gameState.captureCount++;

#ifdef _DEBUG
                // Debug: always respawn, never game over
                constexpr bool shouldGameOver = false;
#else
                bool shouldGameOver = (gameState.captureCount >= gameState.maxCaptures);
#endif

                if (shouldGameOver) {
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
                        auto& respawnState = world.GetComponent<RespawnStateComponent>(gameStateEntity);
                        respawnState.state = RespawnStateComponent::State::FadeOut;
                        respawnState.timer = 0.0f;
                        respawnState.fadeAlpha = 0.0f;
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
