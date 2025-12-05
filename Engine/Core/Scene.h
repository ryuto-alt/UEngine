#pragma once

#include "GameObject.h"
#include "Camera.h"
#include "CameraComponent.h"
#include "../Rendering/RenderView.h"
#include <vector>
#include <memory>
#include <string>

#ifdef _DEBUG
#include "../../Game/UI/EditorUI.h"
#endif

namespace UnoEngine {

class Application;
class InputManager;

class Scene {
public:
    Scene(const std::string& name = "Scene");
    virtual ~Scene() = default;

    void OnLoad();
    virtual void OnUnload() {}
    void OnUpdate(float deltaTime);
    void OnRender(RenderView& view);
    void OnImGui();

    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void DestroyGameObject(GameObject* obj);

    const std::string& GetName() const { return name_; }
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }
    std::vector<std::unique_ptr<GameObject>>& GetGameObjects() { return gameObjects_; }

    Camera* GetActiveCamera() const { return activeCamera_; }
    void SetActiveCamera(Camera* camera) { activeCamera_ = camera; }

    CameraComponent* GetActiveCameraComponent() const { return activeCameraComponent_; }
    void SetActiveCameraComponent(CameraComponent* camComp) { activeCameraComponent_ = camComp; }

    Application* GetApplication() const { return app_; }
    void SetApplication(Application* app) { app_ = app; }

    void SetInputManager(InputManager* input) { input_ = input; }

    // Call Start() on a specific GameObject's components (useful for runtime-created objects)
    void StartGameObject(GameObject* obj);

#ifdef _DEBUG
    EditorUI* GetEditorUI() { return &editorUI_; }
#endif

protected:
    InputManager* input_ = nullptr;

    // Call Start() on all components that haven't started yet
    void ProcessPendingStarts();

private:
    void SetupDefaultCamera();
    void LoadSceneFromFile(const std::string& filepath);

    std::string name_;
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    std::vector<GameObject*> pendingDestroy_;
    Camera* activeCamera_ = nullptr;
    CameraComponent* activeCameraComponent_ = nullptr;
    Application* app_ = nullptr;
    bool isLoaded_ = false;
    GameObject* mainCamera_ = nullptr;

#ifdef _DEBUG
    EditorUI editorUI_;
#endif
};

} // namespace UnoEngine
