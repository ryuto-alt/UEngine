#include "pch.h"
#include "Keyboard.h"

namespace UnoEngine {

void Keyboard::Update() {
    // 前フレームの状態を保存
    previousState_ = currentState_;
}

void Keyboard::ProcessKeyDown(uint32 vkCode) {
    if (vkCode < KEY_COUNT) {
        currentState_[vkCode] = true;
    }
}

void Keyboard::ProcessKeyUp(uint32 vkCode) {
    if (vkCode < KEY_COUNT) {
        currentState_[vkCode] = false;
    }
}

bool Keyboard::IsDown(KeyCode key) const {
    const uint32 index = static_cast<uint32>(key);
    return index < KEY_COUNT && currentState_[index];
}

bool Keyboard::IsPressed(KeyCode key) const {
    const uint32 index = static_cast<uint32>(key);
    return index < KEY_COUNT && currentState_[index] && !previousState_[index];
}

bool Keyboard::IsReleased(KeyCode key) const {
    const uint32 index = static_cast<uint32>(key);
    return index < KEY_COUNT && !currentState_[index] && previousState_[index];
}

void Keyboard::Reset() {
    currentState_.fill(false);
    previousState_.fill(false);
}

} // namespace UnoEngine
