#include "SceneManager.h"

namespace UnoEngine {

void SceneManager::Update(float deltaTime) {
    if (activeScene_) {
        activeScene_->OnUpdate(deltaTime);
    }
}

void SceneManager::LoadScene(std::unique_ptr<Scene> scene) {
    if (activeScene_) {
        activeScene_->OnUnload();
    }
    
    activeScene_ = std::move(scene);
    
    if (activeScene_) {
        activeScene_->OnLoad();
    }
}

} // namespace UnoEngine
