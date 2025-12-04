#include "pch.h"
#include "AnimationSystem.h"
#include "AnimatorComponent.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"

namespace UnoEngine {

void AnimationSystem::OnUpdate(Scene* scene, float deltaTime) {
    if (!scene) return;

#ifdef _DEBUG
    // Debug: 0.1秒後に自動停止
    if (!autoStopTriggered_) {
        elapsedTime_ += deltaTime;
        if (elapsedTime_ >= AUTO_STOP_TIME) {
            isPlaying_ = false;
            autoStopTriggered_ = true;
        }
    }
#endif

    if (!isPlaying_) return;

    const auto& gameObjects = scene->GetGameObjects();

    for (const auto& go : gameObjects) {
        if (!go || !go->IsActive()) continue;

        auto* animator = go->GetComponent<AnimatorComponent>();
        if (animator && animator->IsEnabled()) {
            animator->UpdateAnimation(deltaTime);
        }
    }
}

} // namespace UnoEngine
