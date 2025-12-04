#include "pch.h"
#include "Mouse.h"

namespace UnoEngine {

void Mouse::Update() {
    // 前フレームの状態を保存
    previousState_ = currentState_;

    // 座標の差分を計算
    deltaX_ = x_ - previousX_;
    deltaY_ = y_ - previousY_;
    previousX_ = x_;
    previousY_ = y_;

    // ホイールはフレーム毎にリセット
    wheelDelta_ = 0;
}

void Mouse::ProcessButtonDown(MouseButton button) {
    const size_t index = static_cast<size_t>(button);
    if (index < BUTTON_COUNT) {
        currentState_[index] = true;
    }
}

void Mouse::ProcessButtonUp(MouseButton button) {
    const size_t index = static_cast<size_t>(button);
    if (index < BUTTON_COUNT) {
        currentState_[index] = false;
    }
}

void Mouse::ProcessMove(int32 x, int32 y) {
    x_ = x;
    y_ = y;
}

void Mouse::ProcessWheel(int32 delta) {
    wheelDelta_ += delta;
}

bool Mouse::IsDown(MouseButton button) const {
    const size_t index = static_cast<size_t>(button);
    return index < BUTTON_COUNT && currentState_[index];
}

bool Mouse::IsPressed(MouseButton button) const {
    const size_t index = static_cast<size_t>(button);
    return index < BUTTON_COUNT && currentState_[index] && !previousState_[index];
}

bool Mouse::IsReleased(MouseButton button) const {
    const size_t index = static_cast<size_t>(button);
    return index < BUTTON_COUNT && !currentState_[index] && previousState_[index];
}

void Mouse::Reset() {
    currentState_.fill(false);
    previousState_.fill(false);
    x_ = y_ = 0;
    previousX_ = previousY_ = 0;
    deltaX_ = deltaY_ = 0;
    wheelDelta_ = 0;
}

} // namespace UnoEngine
