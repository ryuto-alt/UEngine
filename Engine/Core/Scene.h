#pragma once

#include "GameObject.h"
#include "Camera.h"
#include "../Graphics/RenderView.h"
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

    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void DestroyGameObject(GameObject* obj);

    const std::string& GetName() const { return name_; }
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }
    
    Camera* GetActiveCamera() const { return activeCamera_; }
    void SetActiveCamera(Camera* camera) { activeCamera_ = camera; }
    
    Application* GetApplication() const { return app_; }
    void SetApplication(Application* app) { app_ = app; }
    
    void SetInputManager(InputManager* input) { input_ = input; }

protected:
    InputManager* input_ = nullptr;

private:
    std::string name_;
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    std::vector<GameObject*> pendingDestroy_;
    Camera* activeCamera_ = nullptr;
    Application* app_ = nullptr;
};

} // namespace UnoEngine
