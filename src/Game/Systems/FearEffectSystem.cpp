#include "FearEffectSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "ECS/Components/PostProcessComponents.h"
#include "UnoEngine.h"
#include <cmath>
#include <algorithm>

namespace ECS {

void FearEffectSystem::Update(World& world, float deltaTime) {
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;

    auto& playerTransform = world.GetComponent<TransformComponent>(playerEntity);

    Entity gameStateEntity = world.FindEntityWith<GameStateComponent>();
    FearEffectComponent* fear = nullptr;
    if (gameStateEntity.IsValid()) {
        fear = &world.GetComponent<FearEffectComponent>(gameStateEntity);
    }

    auto* lightManager = world.GetResource<LightManager*>();

    // Find horror effect from post process chain
    PostProcessChainComponent* ppChain = nullptr;
    world.ForEach<PostProcessChainComponent>(
        [&ppChain](Entity entity, PostProcessChainComponent& pp) {
            ppChain = &pp;
        }
    );

    // Check if any enemy is chasing
    float maxFearIntensity = 0.0f;

    world.ForEach<EnemyTag, TransformComponent, EnemyAIComponent>(
        [&](Entity entity, EnemyTag&, TransformComponent& enemyTransform,
            EnemyAIComponent& ai) {

            if (!ai.isChasing) return;

            float dx = enemyTransform.position.x - playerTransform.position.x;
            float dy = enemyTransform.position.y - playerTransform.position.y;
            float dz = enemyTransform.position.z - playerTransform.position.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

            constexpr float MIN_DISTANCE = 3.0f;
            constexpr float MAX_DISTANCE = 15.0f;

            if (distance < MAX_DISTANCE) {
                float normalized = 1.0f - ((distance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE));
                normalized = std::clamp(normalized, 0.0f, 1.0f);
                maxFearIntensity = (std::max)(maxFearIntensity, normalized);
            }
        }
    );

    // Apply vignette effect
    if (ppChain && ppChain->horrorEffect) {
        static float time = 0.0f;
        time += deltaTime;
        ppChain->horrorEffect->SetHorrorParams(time, 0.0f, 0.0f, 0.0f, maxFearIntensity * 0.8f);
    }

    // Apply fear-based light flicker
    if (lightManager) {
        lightManager->SetFearFlickerIntensity(maxFearIntensity);
    }

    // Store fear values
    if (fear) {
        fear->vignetteIntensity = maxFearIntensity * 0.8f;
        fear->shakeIntensity = maxFearIntensity;
        fear->flickerIntensity = maxFearIntensity;
    }
}

} // namespace ECS
