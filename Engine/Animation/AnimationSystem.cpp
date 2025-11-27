#include "AnimationSystem.h"
#include "AnimatorComponent.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"

namespace UnoEngine {

void AnimationSystem::OnUpdate(Scene* scene, float deltaTime) {
    if (!scene) return;

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
