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

    // Enemy
    world.ForEach<EnemyTag, MeshRendererComponent>(
        [](Entity entity, EnemyTag&, MeshRendererComponent& renderer) {
            if (renderer.object3d) renderer.object3d->Draw();
        }
    );

    // NavMesh visualization
    UnoEngine::GetInstance()->DrawNavVis();
    auto* navMeshManager = UnoEngine::GetInstance()->GetNavMgr();
    if (navMeshManager) {
        navMeshManager->DrawDebugPreview();
    }

    // Post-process chain: PSX → Horror → Backbuffer
    if (ppChain) {
        if (ppChain->psxEffect && ppChain->horrorEffect) {
            if (ppChain->horrorEffect) {
                ppChain->horrorEffect->SetFisheyeStrength(ppChain->fisheyeStrength);
                ppChain->horrorEffect->SetFisheyeRadius(ppChain->fisheyeRadius);
            }
            ppChain->psxEffect->PostDrawTo(ppChain->horrorEffect.get());
            ppChain->horrorEffect->PostDraw();
        } else if (ppChain->psxEffect) {
            ppChain->psxEffect->PostDraw();
        }
    }
}

} // namespace ECS
