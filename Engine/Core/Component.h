#pragma once

#include "Types.h"

namespace UnoEngine {

class GameObject;

class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    // Lifecycle methods (Unity-style)
    virtual void Awake() {}                      // Called immediately after AddComponent
    virtual void Start() {}                      // Called once before first Update (after all Awake)
    virtual void OnUpdate(float deltaTime) {}    // Called every frame
    virtual void OnDestroy() {}                  // Called on RemoveComponent or GameObject destruction

    // State accessors
    GameObject* GetGameObject() const { return gameObject_; }
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    // Lifecycle state
    bool HasStarted() const { return hasStarted_; }
    bool IsAwakeCalled() const { return isAwakeCalled_; }

protected:
    friend class GameObject;
    friend class Scene;

    void MarkAwakeCalled() { isAwakeCalled_ = true; }
    void MarkStarted() { hasStarted_ = true; }

    GameObject* gameObject_ = nullptr;
    bool enabled_ = true;
    bool hasStarted_ = false;
    bool isAwakeCalled_ = false;
};

} // namespace UnoEngine
