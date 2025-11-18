#pragma once

#include "Component.h"
#include "Transform.h"
#include <vector>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace UnoEngine {

class GameObject {
public:
    GameObject(const std::string& name = "GameObject");
    ~GameObject() = default;

    void OnUpdate(float deltaTime);

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    template<typename T>
    T* GetComponent() const;

    template<typename T>
    void RemoveComponent();

    Transform& GetTransform() { return transform_; }
    const Transform& GetTransform() const { return transform_; }

    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }

private:
    std::string name_;
    Transform transform_;
    std::vector<std::unique_ptr<Component>> components_;
    std::unordered_map<std::type_index, Component*> componentMap_;
    bool isActive_ = true;
};

template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = component.get();
    ptr->gameObject_ = this;

    componentMap_[std::type_index(typeid(T))] = ptr;
    components_.push_back(std::move(component));

    return ptr;
}

template<typename T>
T* GameObject::GetComponent() const {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

    auto it = componentMap_.find(std::type_index(typeid(T)));
    if (it != componentMap_.end()) {
        return static_cast<T*>(it->second);
    }
    return nullptr;
}

template<typename T>
void GameObject::RemoveComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

    auto typeIndex = std::type_index(typeid(T));
    auto mapIt = componentMap_.find(typeIndex);
    if (mapIt == componentMap_.end()) return;

    Component* compPtr = mapIt->second;
    componentMap_.erase(mapIt);

    components_.erase(
        std::remove_if(components_.begin(), components_.end(),
            [compPtr](const std::unique_ptr<Component>& c) {
                return c.get() == compPtr;
            }),
        components_.end()
    );
}

} // namespace UnoEngine
