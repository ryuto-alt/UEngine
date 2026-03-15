#pragma once

#include <cstdint>
#include <string>

#include "EventSystem.h"

namespace UnoEngine {

class GameObject;

// シーンが読み込まれた時に発火
struct SceneLoadedEvent : public Event {
    std::string sceneName;

    explicit SceneLoadedEvent(const std::string& name = "")
        : sceneName(name) {}
};

// シーンがアンロードされた時に発火
struct SceneUnloadedEvent : public Event {
    std::string sceneName;

    explicit SceneUnloadedEvent(const std::string& name = "")
        : sceneName(name) {}
};

// GameObjectが生成された時に発火
struct GameObjectCreatedEvent : public Event {
    GameObject* gameObject = nullptr;

    explicit GameObjectCreatedEvent(GameObject* obj = nullptr)
        : gameObject(obj) {}
};

// GameObjectが破棄された時に発火
struct GameObjectDestroyedEvent : public Event {
    std::string objectName;

    explicit GameObjectDestroyedEvent(const std::string& name = "")
        : objectName(name) {}
};

// 入力デバイスの種類
enum class InputDevice {
    Keyboard,
    Gamepad
};

// 入力デバイスが切り替わった時に発火
struct InputDeviceChangedEvent : public Event {
    InputDevice device = InputDevice::Keyboard;

    explicit InputDeviceChangedEvent(InputDevice dev = InputDevice::Keyboard)
        : device(dev) {}
};

// ウィンドウサイズが変更された時に発火
struct WindowResizedEvent : public Event {
    uint32_t width = 0;
    uint32_t height = 0;

    WindowResizedEvent() = default;
    WindowResizedEvent(uint32_t w, uint32_t h)
        : width(w), height(h) {}
};

// 設定が変更された時に発火
struct SettingsChangedEvent : public Event {
    SettingsChangedEvent() = default;
};

// コリジョンが発生した時に発火
struct CollisionEvent : public Event {
    GameObject* objectA = nullptr;
    GameObject* objectB = nullptr;

    CollisionEvent() = default;
    CollisionEvent(GameObject* a, GameObject* b)
        : objectA(a), objectB(b) {}
};

} // namespace UnoEngine
