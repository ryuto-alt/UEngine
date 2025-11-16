#pragma once

#include "Keyboard.h"
#include "Mouse.h"
#include "../Core/NonCopyable.h"

namespace UnoEngine {

// 入力システム全体の管理
class InputManager : public NonCopyable {
public:
    InputManager() = default;
    ~InputManager() = default;

    // フレーム開始時に呼び出す
    void Update();

    // Win32メッセージ処理
    void ProcessMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    // アクセサ
    Keyboard& GetKeyboard() { return keyboard_; }
    const Keyboard& GetKeyboard() const { return keyboard_; }
    Mouse& GetMouse() { return mouse_; }
    const Mouse& GetMouse() const { return mouse_; }

    // すべてリセット
    void Reset();

private:
    Keyboard keyboard_;
    Mouse mouse_;
};

} // namespace UnoEngine
