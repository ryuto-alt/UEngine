#pragma once

#include "Scene.h"
#include <memory>

namespace UnoEngine {

class Application;

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;
    
    void Update(float deltaTime);
    
    void LoadScene(std::unique_ptr<Scene> scene);
    Scene* GetActiveScene() const { return activeScene_.get(); }
    
    void SetApplication(Application* app) { app_ = app; }
    
private:
    std::unique_ptr<Scene> activeScene_;
    Application* app_ = nullptr;
};

} // namespace UnoEngine
