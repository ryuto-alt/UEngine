#include "AnimationSystem.h"
#include "AnimatorComponent.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"
#include <Windows.h>

namespace UnoEngine {

static int frameCount = 0;

void AnimationSystem::OnUpdate(Scene* scene, float deltaTime) {
    if (!scene) return;

    const auto& gameObjects = scene->GetGameObjects();
    
    int animatorCount = 0;
    for (const auto& go : gameObjects) {
        if (!go || !go->IsActive()) continue;

        auto* animator = go->GetComponent<AnimatorComponent>();
        if (animator && animator->IsEnabled()) {
            animator->UpdateAnimation(deltaTime);
            animatorCount++;
        }
    }
    
    // 100フレームに1回だけログ出力
    if (frameCount++ % 100 == 0) {
        char msg[256];
        sprintf_s(msg, "AnimationSystem::OnUpdate - %d animators updated, deltaTime=%.4f\n", 
                  animatorCount, deltaTime);
        OutputDebugStringA(msg);
    }
}

} // namespace UnoEngine
