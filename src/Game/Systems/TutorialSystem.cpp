#include "TutorialSystem.h"
#include "ECS/World.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "ECS/Components/PostProcessComponents.h"
#include "UnoEngine.h"
#include "UI/SubtitleManager.h"

namespace ECS {

void TutorialSystem::Update(World& world, float deltaTime) {
    UnoEngine* engine = UnoEngine::GetInstance();

    world.ForEach<SubtitleUIComponent, TutorialComponent>(
        [engine, deltaTime, &world](Entity entity, SubtitleUIComponent& subtitle,
                                     TutorialComponent& tutorial) {
            if (!subtitle.subtitleManager) return;
            if (tutorial.isFinished) return;

            bool skipPressed = engine->IsKeyTrig(DIK_SPACE);
            bool wasActive = subtitle.subtitleManager->IsActive();

            subtitle.subtitleManager->Update(deltaTime, skipPressed);

            // Tutorial complete: unlock movement, activate enemy, show hint
            if (wasActive && subtitle.subtitleManager->IsFinished()) {
                tutorial.isFinished = true;

                Entity playerEntity = world.FindEntityWith<PlayerTag>();
                if (playerEntity.IsValid()) {
                    auto& jumpscare = world.GetComponent<JumpscareVictimComponent>(playerEntity);
                    jumpscare.isInJumpscare = false;
                }

                // Activate enemy
                world.ForEach<EnemyTag, EnemyAIComponent>(
                    [](Entity e, EnemyTag&, EnemyAIComponent& ai) {
                        ai.isActive = true;
                    }
                );

                subtitle.subtitleManager->ShowHint(L"WASDで移動", 5.0f);
            }
        }
    );
}

} // namespace ECS
