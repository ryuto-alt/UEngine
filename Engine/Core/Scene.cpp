#include "Scene.h"
#include <algorithm>

namespace UnoEngine {

Scene::Scene(const std::string& name)
    : name_(name) {
}

void Scene::OnUpdate(float deltaTime) {
    // Update all game objects
    for (auto& obj : gameObjects_) {
        obj->OnUpdate(deltaTime);
    }

    // Destroy pending objects
    if (!pendingDestroy_.empty()) {
        for (GameObject* obj : pendingDestroy_) {
            gameObjects_.erase(
                std::remove_if(gameObjects_.begin(), gameObjects_.end(),
                    [obj](const std::unique_ptr<GameObject>& go) {
                        return go.get() == obj;
                    }),
                gameObjects_.end()
            );
        }
        pendingDestroy_.clear();
    }
}

GameObject* Scene::CreateGameObject(const std::string& name) {
    auto obj = std::make_unique<GameObject>(name);
    GameObject* ptr = obj.get();
    gameObjects_.push_back(std::move(obj));
    return ptr;
}

void Scene::DestroyGameObject(GameObject* obj) {
    pendingDestroy_.push_back(obj);
}

} // namespace UnoEngine
