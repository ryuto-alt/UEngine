#include "pch.h"
#include "GameObject.h"

namespace UnoEngine {

GameObject::GameObject(const std::string& name)
    : name_(name) {
}

GameObject::~GameObject() {
    // Call OnDestroy for all components before destruction
    for (auto& component : components_) {
        component->OnDestroy();
    }
}

void GameObject::OnUpdate(float deltaTime) {
    if (!isActive_) return;

    for (auto& component : components_) {
        if (component->IsEnabled()) {
            component->OnUpdate(deltaTime);
        }
    }
}

} // namespace UnoEngine
