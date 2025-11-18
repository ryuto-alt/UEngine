#pragma once

#include "GameObject.h"
#include <vector>
#include <memory>
#include <string>

namespace UnoEngine {

class Scene {
public:
    Scene(const std::string& name = "Scene");
    virtual ~Scene() = default;

    virtual void OnLoad() {}
    virtual void OnUnload() {}
    virtual void OnUpdate(float deltaTime);

    GameObject* CreateGameObject(const std::string& name = "GameObject");
    void DestroyGameObject(GameObject* obj);

    const std::string& GetName() const { return name_; }
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return gameObjects_; }

private:
    std::string name_;
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    std::vector<GameObject*> pendingDestroy_;
};

} // namespace UnoEngine
