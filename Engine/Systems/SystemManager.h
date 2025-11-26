#pragma once

#include "ISystem.h"
#include "../Core/Types.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace UnoEngine {

class Scene;

class SystemManager {
public:
    SystemManager() = default;
    ~SystemManager() = default;

    // Register a system (takes ownership)
    template<typename T, typename... Args>
    T* RegisterSystem(Args&&... args);

    // Get a registered system by type
    template<typename T>
    T* GetSystem() const;

    // Called when a scene starts
    void OnSceneStart(Scene* scene);

    // Update all enabled systems in priority order
    void Update(Scene* scene, float deltaTime);

    // Called when a scene ends
    void OnSceneEnd(Scene* scene);

private:
    void SortSystems();

private:
    std::vector<std::unique_ptr<ISystem>> systems_;
    bool needsSort_ = false;
};

template<typename T, typename... Args>
T* SystemManager::RegisterSystem(Args&&... args) {
    static_assert(std::is_base_of<ISystem, T>::value, "T must derive from ISystem");

    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = system.get();
    systems_.push_back(std::move(system));
    needsSort_ = true;
    return ptr;
}

template<typename T>
T* SystemManager::GetSystem() const {
    static_assert(std::is_base_of<ISystem, T>::value, "T must derive from ISystem");

    for (const auto& system : systems_) {
        T* casted = dynamic_cast<T*>(system.get());
        if (casted) {
            return casted;
        }
    }
    return nullptr;
}

} // namespace UnoEngine
