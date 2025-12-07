#include "pch.h"
#include "SceneManager.h"
#include "Application.h"
#include "../Systems/SystemManager.h"

namespace UnoEngine {

void SceneManager::Update(float deltaTime) {
    if (activeScene_) {
        activeScene_->OnUpdate(deltaTime);
    }
}

void SceneManager::LoadScene(std::unique_ptr<Scene> scene) {
    if (activeScene_) {
        activeScene_->OnUnload();
        if (app_) {
            app_->GetSystemManager()->OnSceneEnd(activeScene_.get());
        }
    }

    activeScene_ = std::move(scene);

    if (activeScene_ && app_) {
        activeScene_->SetApplication(app_);
        activeScene_->SetInputManager(app_->GetInput());
    }

    if (activeScene_) {
        activeScene_->OnLoad();
        if (app_) {
            app_->GetSystemManager()->OnSceneStart(activeScene_.get());
        }
    }
}

} // namespace UnoEngine
