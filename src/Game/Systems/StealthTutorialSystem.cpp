#include "StealthTutorialSystem.h"
#include "ECS/World.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "ECS/Components/PostProcessComponents.h"
#include "UnoEngine.h"
#include "UI/SubtitleManager.h"
#include "imgui.h"

namespace ECS {

void StealthTutorialSystem::Update(World& world, float deltaTime) {
    UnoEngine* engine = UnoEngine::GetInstance();

    // Debug: check stealth state
    bool debugEnabled = false;
    bool debugActive = false;
    float debugTimer = 0.0f;
    bool debugTriggered = false;
    bool debugTutActive = false;
    bool debugSubActive = false;
    bool debugSubFinished = false;

    world.ForEach<EnemyTag, StealthComponent>(
        [&](Entity e, EnemyTag&, StealthComponent& stealth) {
            debugEnabled = stealth.stealthEnabled;
            debugActive = stealth.stealthActive;
            debugTimer = stealth.outOfRangeTimer;
        }
    );

    world.ForEach<SubtitleUIComponent, StealthTutorialComponent>(
        [engine, deltaTime, &world, &debugTriggered, &debugTutActive, &debugSubActive, &debugSubFinished](
            Entity entity, SubtitleUIComponent& subtitle,
            StealthTutorialComponent& stealthTutorial) {
            debugTriggered = stealthTutorial.triggered;
            debugTutActive = stealthTutorial.active;

            if (!subtitle.subtitleManager) return;

            debugSubActive = subtitle.subtitleManager->IsActive();
            debugSubFinished = subtitle.subtitleManager->IsFinished();

            // --- Trigger: detect first stealth activation ---
            if (!stealthTutorial.triggered) {
                bool stealthJustActivated = false;
                world.ForEach<EnemyTag, StealthComponent>(
                    [&stealthJustActivated](Entity e, EnemyTag&, StealthComponent& stealth) {
                        if (stealth.stealthActive) {
                            stealthJustActivated = true;
                        }
                    }
                );

                if (!stealthJustActivated) return;

                stealthTutorial.triggered = true;
                stealthTutorial.active = true;

                // Freeze player
                Entity playerEntity = world.FindEntityWith<PlayerTag>();
                if (playerEntity.IsValid()) {
                    auto& jumpscare = world.GetComponent<JumpscareVictimComponent>(playerEntity);
                    jumpscare.isInJumpscare = true;
                    auto& movement = world.GetComponent<PlayerMovementComponent>(playerEntity);
                    movement.isMoving = false;
                    movement.moveDirection = {0.0f, 0.0f, 0.0f};
                }

                // Freeze enemy AI
                world.ForEach<EnemyTag, EnemyAIComponent, PathfindingComponent>(
                    [](Entity e, EnemyTag&, EnemyAIComponent& ai, PathfindingComponent& pathfinding) {
                        ai.isActive = false;
                        pathfinding.currentPath.clear();
                        pathfinding.currentSpeed = 0.0f;
                    }
                );

                // Start subtitle sequence
                subtitle.subtitleManager->SetSteps({
                    {L"おかしい...さっきから足音が聞こえない...", 2.5f, 0.06f},
                    {L"奴の気配がまるでしない...", 2.0f, 0.06f},
                    {L"嫌な予感がする", 2.0f, 0.06f},
                    {L"早くオーブを集めて夢から出よう", 2.0f, 0.06f},
                });
                subtitle.subtitleManager->Start();
                return;
            }

            // --- Active: update subtitle and wait for finish ---
            if (!stealthTutorial.active) return;

            bool skipPressed = engine->IsKeyTrig(DIK_SPACE);
            subtitle.subtitleManager->Update(deltaTime, skipPressed);

            if (subtitle.subtitleManager->IsFinished()) {
                stealthTutorial.active = false;

                // Unfreeze player
                Entity playerEntity = world.FindEntityWith<PlayerTag>();
                if (playerEntity.IsValid()) {
                    auto& jumpscare = world.GetComponent<JumpscareVictimComponent>(playerEntity);
                    jumpscare.isInJumpscare = false;
                }

                // Reactivate enemy AI
                world.ForEach<EnemyTag, EnemyAIComponent>(
                    [](Entity e, EnemyTag&, EnemyAIComponent& ai) {
                        ai.isActive = true;
                    }
                );
            }
        }
    );

#ifdef _DEBUG
    ImGui::Begin("Stealth Tutorial Debug");
    ImGui::Text("Stealth Enabled: %s", debugEnabled ? "YES" : "NO");
    ImGui::Text("Stealth Active: %s", debugActive ? "YES" : "NO");
    ImGui::Text("Stealth Timer: %.1f / 10.0", debugTimer);
    ImGui::Separator();
    ImGui::Text("Tutorial Triggered: %s", debugTriggered ? "YES" : "NO");
    ImGui::Text("Tutorial Active: %s", debugTutActive ? "YES" : "NO");
    ImGui::Text("Subtitle Active: %s", debugSubActive ? "YES" : "NO");
    ImGui::Text("Subtitle Finished: %s", debugSubFinished ? "YES" : "NO");
    ImGui::End();
#endif
}

} // namespace ECS
