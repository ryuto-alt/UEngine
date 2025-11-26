#include "SystemManager.h"
#include "../Core/Scene.h"
#include <algorithm>
#include <Windows.h>

namespace UnoEngine {

static int systemUpdateCount = 0;

void SystemManager::OnSceneStart(Scene* scene) {
    SortSystems();
    
    for (auto& system : systems_) {
        if (system && system->IsEnabled()) {
            system->OnSceneStart(scene);
        }
    }
}

void SystemManager::Update(Scene* scene, float deltaTime) {
    if (needsSort_) {
        SortSystems();
    }

    // 100フレームに1回だけログ出力
    if (systemUpdateCount++ % 100 == 0) {
        char msg[256];
        sprintf_s(msg, "SystemManager::Update - %zu systems registered\n", systems_.size());
        OutputDebugStringA(msg);
    }

    for (auto& system : systems_) {
        if (system && system->IsEnabled()) {
            system->OnUpdate(scene, deltaTime);
        }
    }
}

void SystemManager::OnSceneEnd(Scene* scene) {
    for (auto& system : systems_) {
        if (system && system->IsEnabled()) {
            system->OnSceneEnd(scene);
        }
    }
}

void SystemManager::SortSystems() {
    std::sort(systems_.begin(), systems_.end(),
        [](const std::unique_ptr<ISystem>& a, const std::unique_ptr<ISystem>& b) {
            return a->GetPriority() < b->GetPriority();
        });
    needsSort_ = false;
}

} // namespace UnoEngine
