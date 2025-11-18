#pragma once

#include "Scene.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace UnoEngine {

class Camera;
class InputManager;

class SceneManager {
public:
    static SceneManager& GetInstance();

    template<typename T>
    void RegisterScene(const std::string& name);

    void LoadScene(const std::string& name);
    void OnUpdate(float deltaTime);

    Scene* GetActiveScene() const { return activeScene_.get(); }

    void SetCamera(Camera* camera) { camera_ = camera; }
    void SetInputManager(InputManager* input) { input_ = input; }

    Camera* GetCamera() const { return camera_; }
    InputManager* GetInputManager() const { return input_; }

private:
    SceneManager() = default;
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    using SceneFactory = std::unique_ptr<Scene>(*)();
    std::unordered_map<std::string, SceneFactory> sceneFactories_;
    std::unique_ptr<Scene> activeScene_;

    Camera* camera_ = nullptr;
    InputManager* input_ = nullptr;
};

template<typename T>
void SceneManager::RegisterScene(const std::string& name) {
    static_assert(std::is_base_of<Scene, T>::value, "T must derive from Scene");
    sceneFactories_[name] = []() -> std::unique_ptr<Scene> {
        return std::make_unique<T>();
    };
}

} // namespace UnoEngine
