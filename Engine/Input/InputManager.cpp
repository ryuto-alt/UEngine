#include "pch.h"
#include "InputManager.h"

namespace UnoEngine {

void InputManager::Update() {
    keyboard_.Update();
    mouse_.Update();
}

void InputManager::ProcessMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        // キーボード
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            keyboard_.ProcessKeyDown(static_cast<uint32>(wparam));
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            keyboard_.ProcessKeyUp(static_cast<uint32>(wparam));
            break;

        // マウスボタン
        case WM_LBUTTONDOWN:
            mouse_.ProcessButtonDown(MouseButton::Left);
            break;
        case WM_LBUTTONUP:
            mouse_.ProcessButtonUp(MouseButton::Left);
            break;

        case WM_RBUTTONDOWN:
            mouse_.ProcessButtonDown(MouseButton::Right);
            break;
        case WM_RBUTTONUP:
            mouse_.ProcessButtonUp(MouseButton::Right);
            break;

        case WM_MBUTTONDOWN:
            mouse_.ProcessButtonDown(MouseButton::Middle);
            break;
        case WM_MBUTTONUP:
            mouse_.ProcessButtonUp(MouseButton::Middle);
            break;

        case WM_XBUTTONDOWN:
            if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) {
                mouse_.ProcessButtonDown(MouseButton::X1);
            } else if (GET_XBUTTON_WPARAM(wparam) == XBUTTON2) {
                mouse_.ProcessButtonDown(MouseButton::X2);
            }
            break;

        case WM_XBUTTONUP:
            if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) {
                mouse_.ProcessButtonUp(MouseButton::X1);
            } else if (GET_XBUTTON_WPARAM(wparam) == XBUTTON2) {
                mouse_.ProcessButtonUp(MouseButton::X2);
            }
            break;

        // マウス移動
        case WM_MOUSEMOVE: {
            const int32 x = static_cast<int32>(LOWORD(lparam));
            const int32 y = static_cast<int32>(HIWORD(lparam));
            mouse_.ProcessMove(x, y);
            break;
        }

        // マウスホイール
        case WM_MOUSEWHEEL: {
            const int32 delta = GET_WHEEL_DELTA_WPARAM(wparam);
            mouse_.ProcessWheel(delta);
            break;
        }

        default:
            break;
    }
}

void InputManager::Reset() {
    keyboard_.Reset();
    mouse_.Reset();
}

} // namespace UnoEngine
