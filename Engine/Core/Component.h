#pragma once

#include "Types.h"

namespace UnoEngine {

class GameObject;

class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    virtual void OnUpdate(float deltaTime) {}

    GameObject* GetGameObject() const { return gameObject_; }
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

private:
    friend class GameObject;
    GameObject* gameObject_ = nullptr;
    bool enabled_ = true;
};

} // namespace UnoEngine
