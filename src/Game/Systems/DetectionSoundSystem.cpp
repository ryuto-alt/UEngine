#include "DetectionSoundSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/AudioComponents.h"
#include "ECS/Components/PlayerComponents.h"

namespace ECS {

void DetectionSoundSystem::Update(World& world, float deltaTime) {
    // Get listener from player entity
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;
    if (!world.HasComponent<AudioListenerComponent>(playerEntity)) return;
    auto& listenerComp = world.GetComponent<AudioListenerComponent>(playerEntity);
    if (!listenerComp.listener) return;
    Vector3 listenerPos = listenerComp.listener->GetPosition();
    Vector3 listenerFwd = listenerComp.listener->GetForward();

    world.ForEach<EnemyTag, TransformComponent, EnemyAIComponent, StealthComponent,
                  DetectionSoundComponent>(
        [&listenerPos, &listenerFwd](Entity entity, EnemyTag&, TransformComponent& transform,
           EnemyAIComponent& ai, StealthComponent& stealth,
           DetectionSoundComponent& detection) {

            if (!ai.isActive) return;
            if (!detection.source) return;

            detection.source->SetPosition(transform.position);
            detection.source->Update(listenerPos, listenerFwd);

            // Stealth detection scream on chase start
            bool justStarted = ai.isChasing && !ai.wasChasing;
            if (justStarted && stealth.stealthEnabled) {
                if (detection.source->IsPlaying()) {
                    detection.source->Stop();
                }
                detection.source->SetVolume(0.625f);
                detection.source->Play(false);
                detection.isPlaying = true;
            }

            if (detection.isPlaying && !detection.source->IsPlaying()) {
                detection.isPlaying = false;
            }
        }
    );
}

} // namespace ECS
