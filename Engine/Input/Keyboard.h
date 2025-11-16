#pragma once

#include "../Core/Types.h"
#include <Windows.h>
#include <array>

namespace UnoEngine {

// キーコード定義
enum class KeyCode : uint8 {
    // 文字キー
    A = 0x41, B = 0x42, C = 0x43, D = 0x44, E = 0x45, F = 0x46, G = 0x47, H = 0x48,
    I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C, M = 0x4D, N = 0x4E, O = 0x4F, P = 0x50,
    Q = 0x51, R = 0x52, S = 0x53, T = 0x54, U = 0x55, V = 0x56, W = 0x57, X = 0x58,
    Y = 0x59, Z = 0x5A,

    // 数字キー
    Num0 = 0x30, Num1 = 0x31, Num2 = 0x32, Num3 = 0x33, Num4 = 0x34,
    Num5 = 0x35, Num6 = 0x36, Num7 = 0x37, Num8 = 0x38, Num9 = 0x39,

    // ファンクションキー
    F1 = VK_F1, F2 = VK_F2, F3 = VK_F3, F4 = VK_F4, F5 = VK_F5, F6 = VK_F6,
    F7 = VK_F7, F8 = VK_F8, F9 = VK_F9, F10 = VK_F10, F11 = VK_F11, F12 = VK_F12,

    // 特殊キー
    Escape = VK_ESCAPE,
    Tab = VK_TAB,
    CapsLock = VK_CAPITAL,
    Shift = VK_SHIFT,
    Control = VK_CONTROL,
    Alt = VK_MENU,
    Space = VK_SPACE,
    Enter = VK_RETURN,
    Backspace = VK_BACK,
    Delete = VK_DELETE,

    // 矢印キー
    Left = VK_LEFT,
    Right = VK_RIGHT,
    Up = VK_UP,
    Down = VK_DOWN,

    // その他
    Insert = VK_INSERT,
    Home = VK_HOME,
    End = VK_END,
    PageUp = VK_PRIOR,
    PageDown = VK_NEXT,

    // テンキー
    Numpad0 = VK_NUMPAD0, Numpad1 = VK_NUMPAD1, Numpad2 = VK_NUMPAD2,
    Numpad3 = VK_NUMPAD3, Numpad4 = VK_NUMPAD4, Numpad5 = VK_NUMPAD5,
    Numpad6 = VK_NUMPAD6, Numpad7 = VK_NUMPAD7, Numpad8 = VK_NUMPAD8,
    Numpad9 = VK_NUMPAD9
};

// キーボード入力管理
class Keyboard {
public:
    Keyboard() = default;
    ~Keyboard() = default;

    // フレーム開始時に呼び出す（前フレームの状態を保存）
    void Update();

    // Win32メッセージからの入力処理
    void ProcessKeyDown(uint32 vkCode);
    void ProcessKeyUp(uint32 vkCode);

    // 入力状態取得
    bool IsDown(KeyCode key) const;
    bool IsPressed(KeyCode key) const;  // このフレームで押された
    bool IsReleased(KeyCode key) const; // このフレームで離された

    // すべてのキー状態をリセット
    void Reset();

private:
    static constexpr size_t KEY_COUNT = 256; // 仮想キーコードは0-255

    std::array<bool, KEY_COUNT> currentState_ = {};  // 現在の状態
    std::array<bool, KEY_COUNT> previousState_ = {}; // 前フレームの状態
};

} // namespace UnoEngine
