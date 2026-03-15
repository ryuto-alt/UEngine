#include "pch.h"
#include "Gamepad.h"
#include <cmath>
#include <algorithm>

#pragma comment(lib, "xinput.lib")

namespace UnoEngine {

// XInputボタン定数とGamepadButtonの対応テーブル
static constexpr struct {
    WORD xinputButton;
    GamepadButton gamepadButton;
} kButtonMapping[] = {
    { XINPUT_GAMEPAD_A,              GamepadButton::A },
    { XINPUT_GAMEPAD_B,              GamepadButton::B },
    { XINPUT_GAMEPAD_X,              GamepadButton::X },
    { XINPUT_GAMEPAD_Y,              GamepadButton::Y },
    { XINPUT_GAMEPAD_DPAD_UP,        GamepadButton::DPadUp },
    { XINPUT_GAMEPAD_DPAD_DOWN,      GamepadButton::DPadDown },
    { XINPUT_GAMEPAD_DPAD_LEFT,      GamepadButton::DPadLeft },
    { XINPUT_GAMEPAD_DPAD_RIGHT,     GamepadButton::DPadRight },
    { XINPUT_GAMEPAD_LEFT_SHOULDER,  GamepadButton::LeftShoulder },
    { XINPUT_GAMEPAD_RIGHT_SHOULDER, GamepadButton::RightShoulder },
    { XINPUT_GAMEPAD_LEFT_THUMB,     GamepadButton::LeftThumb },
    { XINPUT_GAMEPAD_RIGHT_THUMB,    GamepadButton::RightThumb },
    { XINPUT_GAMEPAD_START,          GamepadButton::Start },
    { XINPUT_GAMEPAD_BACK,           GamepadButton::Back },
};

void Gamepad::Update() {
    // 前フレームの状態を保存
    previousState_ = currentState_;

    // 未接続時はポーリング頻度を下げる
    if (!connected_) {
        connectionCheckCounter_++;
        if (connectionCheckCounter_ < CONNECTION_CHECK_INTERVAL) {
            return;
        }
        connectionCheckCounter_ = 0;
    }

    // XInputステートを取得
    XINPUT_STATE state = {};
    DWORD result = XInputGetState(playerIndex_, &state);

    if (result == ERROR_SUCCESS) {
        connected_ = true;

        // ボタン状態を更新
        UpdateButtonState(state.Gamepad.wButtons);

        // 左スティック（円形デッドゾーン適用）
        float rawLX = static_cast<float>(state.Gamepad.sThumbLX) / 32767.0f;
        float rawLY = static_cast<float>(state.Gamepad.sThumbLY) / 32767.0f;
        ApplyCircularDeadzone(rawLX, rawLY, stickDeadzone_, leftStickX_, leftStickY_);

        // 右スティック（円形デッドゾーン適用）
        float rawRX = static_cast<float>(state.Gamepad.sThumbRX) / 32767.0f;
        float rawRY = static_cast<float>(state.Gamepad.sThumbRY) / 32767.0f;
        ApplyCircularDeadzone(rawRX, rawRY, stickDeadzone_, rightStickX_, rightStickY_);

        // トリガー（閾値適用）
        leftTrigger_ = ApplyTriggerThreshold(state.Gamepad.bLeftTrigger, triggerThreshold_);
        rightTrigger_ = ApplyTriggerThreshold(state.Gamepad.bRightTrigger, triggerThreshold_);
    } else {
        // 接続が切れた場合、状態をクリア
        if (connected_) {
            connected_ = false;
            currentState_.fill(false);
            leftStickX_ = leftStickY_ = 0.0f;
            rightStickX_ = rightStickY_ = 0.0f;
            leftTrigger_ = rightTrigger_ = 0.0f;
            StopVibration();
        }
    }
}

void Gamepad::UpdateButtonState(WORD buttons) {
    for (const auto& mapping : kButtonMapping) {
        const size_t index = static_cast<size_t>(mapping.gamepadButton);
        currentState_[index] = (buttons & mapping.xinputButton) != 0;
    }
}

float Gamepad::ApplyCircularDeadzone(float x, float y, float deadzone, float& outX, float& outY) {
    float magnitude = std::sqrt(x * x + y * y);

    if (magnitude < deadzone) {
        outX = 0.0f;
        outY = 0.0f;
        return 0.0f;
    }

    // デッドゾーン外の値を0.0~1.0にリマップ
    float normalizedMagnitude = std::min((magnitude - deadzone) / (1.0f - deadzone), 1.0f);

    // 方向を保持したまま正規化
    float scale = normalizedMagnitude / magnitude;
    outX = x * scale;
    outY = y * scale;

    return normalizedMagnitude;
}

float Gamepad::ApplyTriggerThreshold(BYTE rawValue, float threshold) {
    float normalized = static_cast<float>(rawValue) / 255.0f;

    if (normalized < threshold) {
        return 0.0f;
    }

    // 閾値以上の値を0.0~1.0にリマップ
    return std::min((normalized - threshold) / (1.0f - threshold), 1.0f);
}

bool Gamepad::IsDown(GamepadButton button) const {
    const size_t index = static_cast<size_t>(button);
    return index < BUTTON_COUNT && currentState_[index];
}

bool Gamepad::IsPressed(GamepadButton button) const {
    const size_t index = static_cast<size_t>(button);
    return index < BUTTON_COUNT && currentState_[index] && !previousState_[index];
}

bool Gamepad::IsReleased(GamepadButton button) const {
    const size_t index = static_cast<size_t>(button);
    return index < BUTTON_COUNT && !currentState_[index] && previousState_[index];
}

void Gamepad::SetVibration(float leftMotor, float rightMotor) {
    if (!connected_) return;

    XINPUT_VIBRATION vibration = {};
    vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(leftMotor, 0.0f, 1.0f) * 65535.0f);
    vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(rightMotor, 0.0f, 1.0f) * 65535.0f);
    XInputSetState(playerIndex_, &vibration);
}

void Gamepad::StopVibration() {
    XINPUT_VIBRATION vibration = {};
    vibration.wLeftMotorSpeed = 0;
    vibration.wRightMotorSpeed = 0;
    XInputSetState(playerIndex_, &vibration);
}

bool Gamepad::HasInput() const {
    if (!connected_) return false;

    // いずれかのボタンが押されたか
    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        if (currentState_[i]) return true;
    }

    // スティックがデッドゾーン外にあるか
    if (leftStickX_ != 0.0f || leftStickY_ != 0.0f) return true;
    if (rightStickX_ != 0.0f || rightStickY_ != 0.0f) return true;

    // トリガーが閾値を超えているか
    if (leftTrigger_ > 0.0f || rightTrigger_ > 0.0f) return true;

    return false;
}

void Gamepad::Reset() {
    currentState_.fill(false);
    previousState_.fill(false);
    leftStickX_ = leftStickY_ = 0.0f;
    rightStickX_ = rightStickY_ = 0.0f;
    leftTrigger_ = rightTrigger_ = 0.0f;
    connected_ = false;
    connectionCheckCounter_ = 0;
    StopVibration();
}

} // namespace UnoEngine
