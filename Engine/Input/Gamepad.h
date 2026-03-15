#pragma once

#include "../Core/Types.h"
#include <Windows.h>
#include <Xinput.h>
#include <array>

namespace UnoEngine {

// ゲームパッドボタン定義
enum class GamepadButton : uint8 {
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    DPadUp = 4,
    DPadDown = 5,
    DPadLeft = 6,
    DPadRight = 7,
    LeftShoulder = 8,
    RightShoulder = 9,
    LeftThumb = 10,
    RightThumb = 11,
    Start = 12,
    Back = 13,
    Count = 14
};

// ゲームパッド入力管理
class Gamepad {
public:
    Gamepad() = default;
    ~Gamepad() = default;

    // フレーム開始時に呼び出す（XInputポーリング）
    void Update();

    // ボタン状態取得
    bool IsDown(GamepadButton button) const;
    bool IsPressed(GamepadButton button) const;  // このフレームで押された
    bool IsReleased(GamepadButton button) const; // このフレームで離された

    // アナログスティック（-1.0 ~ 1.0、デッドゾーン適用済み）
    float GetLeftStickX() const { return leftStickX_; }
    float GetLeftStickY() const { return leftStickY_; }
    float GetRightStickX() const { return rightStickX_; }
    float GetRightStickY() const { return rightStickY_; }

    // トリガー（0.0 ~ 1.0、閾値適用済み）
    float GetLeftTrigger() const { return leftTrigger_; }
    float GetRightTrigger() const { return rightTrigger_; }

    // 接続状態
    bool IsConnected() const { return connected_; }

    // 振動制御
    void SetVibration(float leftMotor, float rightMotor);
    void StopVibration();

    // デッドゾーン・閾値設定
    void SetStickDeadzone(float deadzone) { stickDeadzone_ = deadzone; }
    void SetTriggerThreshold(float threshold) { triggerThreshold_ = threshold; }

    // このフレームでゲームパッド入力があったか
    bool HasInput() const;

    // すべての状態をリセット
    void Reset();

    // プレイヤーインデックス設定
    void SetPlayerIndex(DWORD index) { playerIndex_ = index; }
    DWORD GetPlayerIndex() const { return playerIndex_; }

private:
    static constexpr size_t BUTTON_COUNT = static_cast<size_t>(GamepadButton::Count);

    // XInputボタン定数からGamepadButtonへのマッピング
    void UpdateButtonState(WORD buttons);

    // アナログ値の正規化
    float ApplyCircularDeadzone(float x, float y, float deadzone, float& outX, float& outY);
    float ApplyTriggerThreshold(BYTE rawValue, float threshold);

    std::array<bool, BUTTON_COUNT> currentState_ = {};
    std::array<bool, BUTTON_COUNT> previousState_ = {};

    float leftStickX_ = 0.0f;
    float leftStickY_ = 0.0f;
    float rightStickX_ = 0.0f;
    float rightStickY_ = 0.0f;
    float leftTrigger_ = 0.0f;
    float rightTrigger_ = 0.0f;

    float stickDeadzone_ = 0.24f;   // XInput推奨値に近い値
    float triggerThreshold_ = 0.12f; // トリガー閾値

    bool connected_ = false;
    DWORD playerIndex_ = 0;

    // 接続チェック頻度制御（未接続時に毎フレームポーリングしない）
    int32 connectionCheckCounter_ = 0;
    static constexpr int32 CONNECTION_CHECK_INTERVAL = 60; // 約1秒ごと
};

} // namespace UnoEngine
