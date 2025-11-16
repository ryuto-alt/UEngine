#pragma once

#include "../Core/Types.h"
#include <Windows.h>
#include <array>

namespace UnoEngine {

// マウスボタン定義
enum class MouseButton : uint8 {
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
    Count = 5
};

// マウス入力管理
class Mouse {
public:
    Mouse() = default;
    ~Mouse() = default;

    // フレーム開始時に呼び出す
    void Update();

    // Win32メッセージからの入力処理
    void ProcessButtonDown(MouseButton button);
    void ProcessButtonUp(MouseButton button);
    void ProcessMove(int32 x, int32 y);
    void ProcessWheel(int32 delta);

    // ボタン状態取得
    bool IsDown(MouseButton button) const;
    bool IsPressed(MouseButton button) const;  // このフレームで押された
    bool IsReleased(MouseButton button) const; // このフレームで離された

    // 座標取得
    int32 GetX() const { return x_; }
    int32 GetY() const { return y_; }
    int32 GetDeltaX() const { return deltaX_; }
    int32 GetDeltaY() const { return deltaY_; }

    // ホイール
    int32 GetWheelDelta() const { return wheelDelta_; }

    // すべての状態をリセット
    void Reset();

private:
    static constexpr size_t BUTTON_COUNT = static_cast<size_t>(MouseButton::Count);

    std::array<bool, BUTTON_COUNT> currentState_ = {};
    std::array<bool, BUTTON_COUNT> previousState_ = {};

    int32 x_ = 0;
    int32 y_ = 0;
    int32 previousX_ = 0;
    int32 previousY_ = 0;
    int32 deltaX_ = 0;
    int32 deltaY_ = 0;

    int32 wheelDelta_ = 0; // ホイールの移動量（1フレーム分）
};

} // namespace UnoEngine
