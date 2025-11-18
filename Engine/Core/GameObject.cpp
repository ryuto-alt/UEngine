#include "GameObject.h"

namespace UnoEngine {

GameObject::GameObject(const std::string& name)
    : name_(name) {
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
