#include "SprintSystem.h"
#include "ECS/World.h"
#include "ECS/Components/PlayerComponents.h"
#include "Camera.h"
#include "UnoEngine.h"
#include <dinput.h>

namespace ECS {

void SprintSystem::Update(World& world, float deltaTime) {
    UnoEngine* engine = UnoEngine::GetInstance();
    Camera* camera = world.GetResource<Camera*>();

    world.ForEach<PlayerTag, SprintComponent, PlayerMovementComponent>(
        [&](Entity, PlayerTag&, SprintComponent& sprint, PlayerMovementComponent& movement) {
            const float kFade = SprintComponent::kFadeDuration;

            // E key → activate sprint
            if (engine->IsKeyTrig(DIK_E) && !sprint.isSprinting) {
                sprint.isSprinting = true;
                sprint.remainingDuration = SprintComponent::kDuration;
                sprint.sprintTime = 0.0f;
                if (camera) {
                    sprint.baseFov = camera->GetFovY();
                }
            }

            if (!sprint.isSprinting) {
                movement.isSprinting = false;
                sprint.effectIntensity = 0.0f;

                // Restore FOV smoothly
                if (camera && sprint.baseFov > 0.0f) {
                    float currentFov = camera->GetFovY();
                    float targetFov = sprint.baseFov;
                    float diff = targetFov - currentFov;
                    if (diff * diff > 0.0001f) {
                        camera->SetFov(currentFov + diff * deltaTime * 8.0f);
                    } else {
                        camera->SetFov(targetFov);
                    }
                }
                return;
            }

            sprint.remainingDuration -= deltaTime;
            sprint.sprintTime += deltaTime;

            if (sprint.remainingDuration <= 0.0f) {
                sprint.isSprinting = false;
                sprint.remainingDuration = 0.0f;
                sprint.effectIntensity = 0.0f;
                movement.isSprinting = false;
                return;
            }

            movement.isSprinting = true;

            // Fade in / maintain / fade out
            float elapsed = SprintComponent::kDuration - sprint.remainingDuration;
            if (elapsed < kFade) {
                sprint.effectIntensity = elapsed / kFade;
            } else if (sprint.remainingDuration < kFade) {
                sprint.effectIntensity = sprint.remainingDuration / kFade;
            } else {
                sprint.effectIntensity = 1.0f;
            }

            // FOV widening: lerp toward boosted FOV
            if (camera && sprint.baseFov > 0.0f) {
                float targetFov = sprint.baseFov + SprintComponent::kFovBoost * sprint.effectIntensity;
                float currentFov = camera->GetFovY();
                float diff = targetFov - currentFov;
                camera->SetFov(currentFov + diff * deltaTime * 6.0f);
            }
        }
    );
}

} // namespace ECS
