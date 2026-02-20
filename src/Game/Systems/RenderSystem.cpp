#include "RenderSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/CollectibleComponents.h"
#include "ECS/Components/CameraComponents.h"
#include "ECS/Components/PostProcessComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "UnoEngine.h"
#ifdef _DEBUG
#include "LineRenderer.h"
#include "Animation/AnimatedModel.h"
#include <cmath>
#endif

namespace ECS {

void RenderSystem::Update(World& world, float deltaTime) {
    Camera* camera = world.GetResource<Camera*>();

    // Get post process chain
    PostProcessChainComponent* ppChain = nullptr;
    world.ForEach<PostProcessChainComponent>(
        [&ppChain](Entity entity, PostProcessChainComponent& pp) {
            ppChain = &pp;
        }
    );

    // Begin post-process render target
    if (ppChain && ppChain->psxEffect) {
        ppChain->psxEffect->PreDraw();
    }

    // Skybox
    world.ForEach<SkyboxComponent>(
        [camera](Entity entity, SkyboxComponent& skybox) {
            if (skybox.enabled && skybox.skybox && camera) {
                skybox.skybox->Draw(camera);
            }
        }
    );

    // SpriteCommon draw setup
    auto* spriteCommon = world.GetResource<SpriteCommon*>();
    if (spriteCommon) {
        spriteCommon->CommonDraw();
    }

    // Check FPS mode for player visibility
    bool isFPSMode = false;
    world.ForEach<PlayerTag, FPSCameraComponent>(
        [&isFPSMode](Entity entity, PlayerTag&, FPSCameraComponent& fpsCam) {
            isFPSMode = fpsCam.isFPSMode;
        }
    );

    // Scene objects (non-player, non-enemy, non-orb)
    world.ForEach<SceneObjectTag, MeshRendererComponent>(
        [camera](Entity entity, SceneObjectTag&, MeshRendererComponent& renderer) {
            if (!renderer.object3d || !renderer.visible) return;
            renderer.object3d->Draw(camera, nullptr, nullptr);
        }
    );

    // Player (hidden in FPS mode)
    if (!isFPSMode) {
        world.ForEach<PlayerTag, MeshRendererComponent>(
            [](Entity entity, PlayerTag&, MeshRendererComponent& renderer) {
                if (renderer.object3d) renderer.object3d->Draw();
            }
        );
    }

    // Orbs
    world.ForEach<CollectibleTag, MeshRendererComponent, OrbComponent>(
        [](Entity entity, CollectibleTag&, MeshRendererComponent& renderer, OrbComponent& orb) {
            if (!orb.isCollected && renderer.object3d) {
                renderer.object3d->Draw();
            }
        }
    );

    // Enemy (only render when active)
    world.ForEach<EnemyTag, MeshRendererComponent, EnemyAIComponent>(
        [](Entity entity, EnemyTag&, MeshRendererComponent& renderer, EnemyAIComponent& ai) {
            if (!ai.isActive) return;
            if (renderer.object3d) renderer.object3d->Draw();
        }
    );

    // Enemy sense X-ray pass (Q ability: red haze through walls)
    float senseAlpha = 0.0f;
    world.ForEach<PlayerTag, EnemySenseComponent>(
        [&senseAlpha](Entity, PlayerTag&, EnemySenseComponent& sense) {
            senseAlpha = sense.fadeAlpha;
        }
    );
    if (senseAlpha > 0.0f) {
        world.ForEach<EnemyTag, MeshRendererComponent, EnemyAIComponent>(
            [senseAlpha](Entity, EnemyTag&, MeshRendererComponent& renderer, EnemyAIComponent& ai) {
                if (!ai.isActive || !renderer.object3d) return;
                renderer.object3d->DrawXRay(senseAlpha);
            }
        );
    }

    // NavMesh visualization
    UnoEngine::GetInstance()->DrawNavVis();
    auto* navMeshManager = UnoEngine::GetInstance()->GetNavMgr();
    if (navMeshManager) {
        navMeshManager->DrawDebugPreview();
    }

#ifdef _DEBUG
    // Enemy foot bone debug grid
    world.ForEach<EnemyTag, TransformComponent, RotationSmoothingComponent,
                  AnimatedModelComponent, EnemyAIComponent, EnemyDebugComponent>(
        [](Entity entity, EnemyTag&, TransformComponent& transform,
           RotationSmoothingComponent& rot, AnimatedModelComponent& animModel,
           EnemyAIComponent& ai, EnemyDebugComponent& debugComp) {

            if (!ai.isActive || !animModel.animatedModel || !debugComp.lineRenderer) return;

            const Skeleton& skeleton = animModel.animatedModel->GetSkeleton();

            auto leftFootIt = skeleton.jointMap.find("mixamorig:LeftToeBase");
            auto rightFootIt = skeleton.jointMap.find("mixamorig:RightToeBase");
            if (leftFootIt == skeleton.jointMap.end()) {
                leftFootIt = skeleton.jointMap.find("mixamorig:LeftFoot");
            }
            if (rightFootIt == skeleton.jointMap.end()) {
                rightFootIt = skeleton.jointMap.find("mixamorig:RightFoot");
            }
            if (leftFootIt == skeleton.jointMap.end() || rightFootIt == skeleton.jointMap.end()) return;

            const Joint& leftFootJoint = skeleton.joints[leftFootIt->second];
            const Joint& rightFootJoint = skeleton.joints[rightFootIt->second];

            Vector3 leftFootSkeletonPos = {
                leftFootJoint.skeletonSpaceMatrix.m[3][0],
                leftFootJoint.skeletonSpaceMatrix.m[3][1],
                leftFootJoint.skeletonSpaceMatrix.m[3][2]
            };
            Vector3 rightFootSkeletonPos = {
                rightFootJoint.skeletonSpaceMatrix.m[3][0],
                rightFootJoint.skeletonSpaceMatrix.m[3][1],
                rightFootJoint.skeletonSpaceMatrix.m[3][2]
            };

            constexpr float modelScale = 0.05f;
            Vector3 leftFootWorldPos = {
                transform.position.x + leftFootSkeletonPos.x * modelScale,
                transform.position.y + leftFootSkeletonPos.y * modelScale,
                transform.position.z + leftFootSkeletonPos.z * modelScale
            };
            Vector3 rightFootWorldPos = {
                transform.position.x + rightFootSkeletonPos.x * modelScale,
                transform.position.y + rightFootSkeletonPos.y * modelScale,
                transform.position.z + rightFootSkeletonPos.z * modelScale
            };

            constexpr float GROUND_HEIGHT = 0.0f;
            constexpr float FOOT_GROUND_THRESHOLD = 0.5f;
            bool leftGrounded = leftFootWorldPos.y <= GROUND_HEIGHT + FOOT_GROUND_THRESHOLD;
            bool rightGrounded = rightFootWorldPos.y <= GROUND_HEIGHT + FOOT_GROUND_THRESHOLD;

            Vector4 leftColor = leftGrounded ? Vector4{0.0f, 1.0f, 0.0f, 1.0f} : Vector4{1.0f, 0.0f, 0.0f, 1.0f};
            Vector4 rightColor = rightGrounded ? Vector4{0.0f, 1.0f, 0.0f, 1.0f} : Vector4{1.0f, 0.0f, 0.0f, 1.0f};

            debugComp.lineRenderer->Clear();

            constexpr float gridSize = 1.0f;
            constexpr int gridDivisions = 10;
            constexpr float cellSize = gridSize / gridDivisions;

            float rotationY = rot.currentRotationY;
            float cosRot = std::cos(rotationY);
            float sinRot = std::sin(rotationY);

            // Draw grids for both feet
            Vector3 footPositions[2] = {leftFootWorldPos, rightFootWorldPos};
            Vector4 footColors[2] = {leftColor, rightColor};

            for (int foot = 0; foot < 2; ++foot) {
                const Vector3& footPos = footPositions[foot];
                const Vector4& color = footColors[foot];

                for (int i = 0; i <= gridDivisions; ++i) {
                    float offset = -gridSize / 2.0f + i * cellSize;

                    // X-direction lines (rotated)
                    float localX = offset;
                    float localZStart = -gridSize / 2.0f;
                    float localZEnd = gridSize / 2.0f;

                    Vector3 start1 = {
                        footPos.x + localX * cosRot - localZStart * sinRot,
                        GROUND_HEIGHT,
                        footPos.z + localX * sinRot + localZStart * cosRot
                    };
                    Vector3 end1 = {
                        footPos.x + localX * cosRot - localZEnd * sinRot,
                        GROUND_HEIGHT,
                        footPos.z + localX * sinRot + localZEnd * cosRot
                    };
                    debugComp.lineRenderer->AddLine(start1, end1, color);

                    // Z-direction lines (rotated)
                    float localXStart = -gridSize / 2.0f;
                    float localXEnd = gridSize / 2.0f;
                    float localZ = offset;

                    Vector3 start2 = {
                        footPos.x + localXStart * cosRot - localZ * sinRot,
                        GROUND_HEIGHT,
                        footPos.z + localXStart * sinRot + localZ * cosRot
                    };
                    Vector3 end2 = {
                        footPos.x + localXEnd * cosRot - localZ * sinRot,
                        GROUND_HEIGHT,
                        footPos.z + localXEnd * sinRot + localZ * cosRot
                    };
                    debugComp.lineRenderer->AddLine(start2, end2, color);
                }

                // Vertical line from foot to ground
                debugComp.lineRenderer->AddLine(footPos, Vector3{footPos.x, GROUND_HEIGHT, footPos.z}, color);
            }

            debugComp.lineRenderer->Render();
        }
    );
#endif

    // Update sprint effect params from player SprintComponent
    if (ppChain && ppChain->sprintEffect) {
        float sprintIntensity = 0.0f;
        float sprintTime = 0.0f;
        world.ForEach<PlayerTag, SprintComponent>(
            [&sprintIntensity, &sprintTime](Entity, PlayerTag&, SprintComponent& sprint) {
                sprintIntensity = sprint.effectIntensity;
                sprintTime = sprint.sprintTime;
            }
        );
        float screenAspect = camera ? camera->GetAspectRatio() : (16.0f / 9.0f);
        ppChain->sprintEffect->SetSprintParams(sprintIntensity, sprintTime,
                                               screenAspect, 4.0f / 3.0f);
    }

    // Post-process chain: PSX → Horror → CRT → Sprint → Backbuffer
    if (ppChain) {
        if (ppChain->psxEffect && ppChain->horrorEffect && ppChain->crtEffect) {
            ppChain->horrorEffect->SetFisheyeStrength(ppChain->fisheyeStrength);
            ppChain->horrorEffect->SetFisheyeRadius(ppChain->fisheyeRadius);
            ppChain->psxEffect->PostDrawTo(ppChain->horrorEffect.get());
            ppChain->horrorEffect->PostDrawTo(ppChain->crtEffect.get());
            if (ppChain->sprintEffect) {
                ppChain->crtEffect->PostDrawTo(ppChain->sprintEffect.get());
                ppChain->sprintEffect->PostDraw();
            } else {
                ppChain->crtEffect->PostDraw();
            }
        } else if (ppChain->psxEffect && ppChain->horrorEffect) {
            ppChain->horrorEffect->SetFisheyeStrength(ppChain->fisheyeStrength);
            ppChain->horrorEffect->SetFisheyeRadius(ppChain->fisheyeRadius);
            ppChain->psxEffect->PostDrawTo(ppChain->horrorEffect.get());
            ppChain->horrorEffect->PostDraw();
        } else if (ppChain->psxEffect) {
            ppChain->psxEffect->PostDraw();
        }
    }
}

} // namespace ECS
