#pragma once

#include "Scene.h"
#include <memory>

namespace UnoEngine {

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;
    
    void Update(float deltaTime);
    
    void LoadScene(std::unique_ptr<Scene> scene);
    Scene* GetActiveScene() const { return activeScene_.get(); }
    
private:
    std::unique_ptr<Scene> activeScene_;
};

} // namespace UnoEngine
