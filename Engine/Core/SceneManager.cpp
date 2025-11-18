#include "SceneManager.h"
#include <stdexcept>

namespace UnoEngine {

SceneManager& SceneManager::GetInstance() {
    static SceneManager instance;
    return instance;
}

void SceneManager::LoadScene(const std::string& name) {
    auto it = sceneFactories_.find(name);
    if (it == sceneFactories_.end()) {
        throw std::runtime_error("Scene not registered: " + name);
    }

    if (activeScene_) {
        activeScene_->OnUnload();
    }

    activeScene_ = it->second();
    activeScene_->OnLoad();
}

void SceneManager::OnUpdate(float deltaTime) {
    if (activeScene_) {
        activeScene_->OnUpdate(deltaTime);
    }
}

} // namespace UnoEngine
