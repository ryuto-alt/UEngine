#include "BarkSoundSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/AudioComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include <random>

namespace ECS {

void BarkSoundSystem::Update(World& world, float deltaTime) {
    // Get listener from player entity
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;
    if (!world.HasComponent<AudioListenerComponent>(playerEntity)) return;
    auto& listenerComp = world.GetComponent<AudioListenerComponent>(playerEntity);
    if (!listenerComp.listener) return;
    Vector3 listenerPos = listenerComp.listener->GetPosition();
    Vector3 listenerFwd = listenerComp.listener->GetForward();

    world.ForEach<EnemyTag, TransformComponent, EnemyAIComponent,
                  BarkSoundComponent>(
        [deltaTime, &listenerPos, &listenerFwd](Entity entity, EnemyTag&, TransformComponent& transform,
                   EnemyAIComponent& ai, BarkSoundComponent& bark) {

            if (!bark.source) return;

            bark.source->SetPosition(transform.position);
            bark.source->Update(listenerPos, listenerFwd);

            bark.totalTime += deltaTime;

            bool justStarted = ai.isChasing && !ai.wasChasing;

            // Bark immediately on chase start
            if (justStarted) {
                if (bark.source->IsPlaying()) bark.source->Stop();
                bark.source->Play(false);
                bark.lastBarkTime = bark.totalTime;

                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_real_distribution<float> dist(bark.minInterval, bark.maxInterval);
                bark.nextBarkInterval = dist(gen);
                return;
            }

            if (!ai.isChasing) return;

            // Periodic barking during chase
            if (bark.totalTime - bark.lastBarkTime >= bark.nextBarkInterval) {
                if (!bark.source->IsPlaying()) {
                    bark.source->Play(false);
                    bark.lastBarkTime = bark.totalTime;

                    static std::random_device rd;
                    static std::mt19937 gen(rd());
                    std::uniform_real_distribution<float> dist(bark.minInterval, bark.maxInterval);
                    bark.nextBarkInterval = dist(gen);
                }
            }
        }
    );
}

} // namespace ECS
