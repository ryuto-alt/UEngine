#include "ChaseBGMSystem.h"
#include "ECS/World.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/AudioComponents.h"
#include "UnoEngine.h"

namespace ECS {

void ChaseBGMSystem::Update(World& world, float deltaTime) {
    UnoEngine* engine = UnoEngine::GetInstance();
    if (!engine) return;

    world.ForEach<EnemyTag, EnemyAIComponent, ChaseBGMComponent>(
        [engine, deltaTime](Entity entity, EnemyTag&, EnemyAIComponent& ai,
                           ChaseBGMComponent& bgm) {

            // First-time load
            if (!bgm.loaded) {
                engine->LoadAudio("chaseBGM", "Resources/audio/bgm/chase.mp3");
                bgm.loaded = true;
            }

            // Chase started: begin fade in
            if (ai.isChasing && !bgm.playing) {
                if (engine->IsAudPlay("stagebgm")) {
                    engine->StopAudio("stagebgm");
                }
                engine->PlayAudio("chaseBGM", true, 0.0f);
                bgm.playing = true;
                bgm.volume = 0.0f;
                bgm.targetVolume = bgm.maxVolume;
                bgm.fadingIn = true;
                bgm.fadingOut = false;
            }
            // Chase ended: begin fade out
            else if (!ai.isChasing && bgm.playing && !bgm.fadingOut) {
                bgm.targetVolume = 0.0f;
                bgm.fadingOut = true;
                bgm.fadingIn = false;
            }

            // Fade in
            if (bgm.fadingIn) {
                float fadeSpeed = bgm.maxVolume / bgm.fadeInDuration;
                bgm.volume += fadeSpeed * deltaTime;
                if (bgm.volume >= bgm.targetVolume) {
                    bgm.volume = bgm.targetVolume;
                    bgm.fadingIn = false;
                }
                engine->SetAudVol("chaseBGM", bgm.volume);
            }

            // Fade out
            if (bgm.fadingOut) {
                float fadeSpeed = bgm.maxVolume / bgm.fadeOutDuration;
                bgm.volume -= fadeSpeed * deltaTime;
                if (bgm.volume <= 0.0f) {
                    bgm.volume = 0.0f;
                    bgm.fadingOut = false;
                    bgm.playing = false;
                    engine->StopAudio("chaseBGM");

                    // Resume stage BGM
                    if (!engine->IsAudPlay("stagebgm")) {
                        engine->PlayAudio("stagebgm", true, 0.0375f);
                    }
                } else {
                    engine->SetAudVol("chaseBGM", bgm.volume);
                }
            }
        }
    );
}

} // namespace ECS
