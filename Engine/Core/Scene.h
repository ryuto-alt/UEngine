#pragma once

#include "GameObject.h"
#include "Camera.h"
#include "../Rendering/RenderView.h"
#include <vector>
#include <memory>
#include <string>

namespace UnoEngine {

class Application;
class InputManager;

class Scene {
public:
    Scene(const std::string& name = "Scene");
    virtual ~Scene() = default;

    virtual void OnLoad() {}
    virtual void OnUnload() {}
    virtual void OnUpdate(float deltaTime);
    virtual void OnRender(RenderView& view) = 0;
    virtual void OnImGui() {}

    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void DestroyGameObject(GameObject* obj);

    const std::string& GetName() const { return name_; }
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }
    std::vector<std::unique_ptr<GameObject>>& GetGameObjects() { return gameObjects_; }

    Camera* GetActiveCamera() const { return activeCamera_; }
    void SetActiveCamera(Camera* camera) { activeCamera_ = camera; }
    
    Application* GetApplication() const { return app_; }
    void SetApplication(Application* app) { app_ = app; }
    
    void SetInputManager(InputManager* input) { input_ = input; }

protected:
    InputManager* input_ = nullptr;

    // Call Start() on all components that haven't started yet
    void ProcessPendingStarts();

private:
    std::string name_;
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    std::vector<GameObject*> pendingDestroy_;
    Camera* activeCamera_ = nullptr;
    Application* app_ = nullptr;
    bool isLoaded_ = false;
};

} // namespace UnoEngine
