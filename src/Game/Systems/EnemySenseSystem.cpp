#include "EnemySenseSystem.h"
#include "ECS/World.h"
#include "ECS/Components/PlayerComponents.h"
#include "UnoEngine.h"
#include <dinput.h>

namespace ECS {

void EnemySenseSystem::Update(World& world, float deltaTime) {
    UnoEngine* engine = UnoEngine::GetInstance();

    world.ForEach<PlayerTag, EnemySenseComponent>(
        [&](Entity entity, PlayerTag&, EnemySenseComponent& sense) {
            const float kFade = EnemySenseComponent::kFadeDuration;

            // Count down cooldown
            if (sense.remainingCooldown > 0.0f) {
                sense.remainingCooldown -= deltaTime;
                if (sense.remainingCooldown < 0.0f) sense.remainingCooldown = 0.0f;
            }

            // Q key triggers ability
            if (engine->IsKeyTrig(DIK_Q) && !sense.isActive && sense.remainingCooldown <= 0.0f) {
                sense.isActive = true;
                sense.remainingActive = EnemySenseComponent::kActiveDuration;
            }

            if (!sense.isActive) {
                sense.fadeAlpha = 0.0f;
                return;
            }

            sense.remainingActive -= deltaTime;

            if (sense.remainingActive <= 0.0f) {
                sense.isActive          = false;
                sense.remainingActive   = 0.0f;
                sense.fadeAlpha         = 0.0f;
                sense.remainingCooldown = EnemySenseComponent::kCooldownDuration;
                return;
            }

            // Elapsed since activation
            float elapsed = EnemySenseComponent::kActiveDuration - sense.remainingActive;

            if (elapsed < kFade) {
                // Fade in
                sense.fadeAlpha = elapsed / kFade;
            } else if (sense.remainingActive < kFade) {
                // Fade out
                sense.fadeAlpha = sense.remainingActive / kFade;
            } else {
                sense.fadeAlpha = 1.0f;
            }
        }
    );
}

} // namespace ECS
