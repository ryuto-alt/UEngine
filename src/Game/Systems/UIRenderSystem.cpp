#include "UIRenderSystem.h"
#include "ECS/World.h"
#include "ECS/Components/PostProcessComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/CollectibleComponents.h"
#include "UnoEngine.h"
#include "UI/Minimap.h"
#include "UI/SubtitleManager.h"

namespace ECS {

void UIRenderSystem::Update(World& world, float deltaTime) {
    auto* spriteCommon = world.GetResource<SpriteCommon*>();

    // Minimap - gather ECS data then draw (only after tutorial)
    world.ForEach<MinimapComponent>(
        [&world](Entity entity, MinimapComponent& minimap) {
            if (!minimap.minimap) return;

            // Check if tutorial is finished
            bool tutorialFinished = true;
            world.ForEach<TutorialComponent>(
                [&tutorialFinished](Entity e, TutorialComponent& tutorial) {
                    if (!tutorial.isFinished) {
                        tutorialFinished = false;
                    }
                }
            );

            // Don't draw minimap during tutorial
            if (!tutorialFinished) return;

            // Player position
            Entity playerEntity = world.FindEntityWith<PlayerTag>();
            Vector3 playerPos{};
            if (playerEntity.IsValid()) {
                playerPos = world.GetComponent<TransformComponent>(playerEntity).position;
            }

            // Orb data
            std::vector<Vector3> uncollectedPositions;
            int totalOrbs = 0;
            int collectedOrbs = 0;
            world.ForEach<TransformComponent, OrbComponent>(
                [&](Entity e, TransformComponent& t, OrbComponent& orb) {
                    totalOrbs++;
                    if (orb.isCollected) {
                        collectedOrbs++;
                    } else {
                        uncollectedPositions.push_back(t.position);
                    }
                }
            );

            minimap.minimap->UpdateState(playerPos, uncollectedPositions, totalOrbs, collectedOrbs);
            minimap.minimap->Draw();
        }
    );

    // Subtitles
    world.ForEach<SubtitleUIComponent>(
        [](Entity entity, SubtitleUIComponent& subtitle) {
            if (subtitle.subtitleManager &&
                (subtitle.subtitleManager->IsActive() || subtitle.subtitleManager->IsHintActive())) {
                subtitle.subtitleManager->Draw();
            }
        }
    );

    // Fade sprite (respawn effect)
    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    if (gameStateEntity.IsValid()) {
        auto& respawn = world.GetComponent<RespawnStateComponent>(gameStateEntity);

        if (respawn.fadeAlpha > 0.0f) {
            world.ForEach<FadeSpriteComponent>(
                [&respawn, spriteCommon](Entity entity, FadeSpriteComponent& fade) {
                    if (!fade.sprite) return;
                    fade.sprite->setColor({0.0f, 0.0f, 0.0f, respawn.fadeAlpha});
                    fade.sprite->Update();
                    if (spriteCommon) spriteCommon->CommonDraw();
                    fade.sprite->Draw();
                }
            );
        }
    }
}

} // namespace ECS
