#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include <Windows.h>
#include <functional>

namespace UnoEngine {

// ウィンドウ設定
struct WindowConfig {
    std::wstring title = L"UnoEngine";
    uint32 width = 1280;
    uint32 height = 720;
    bool fullscreen = false;
};

// ウィンドウクラス
class Window : public NonCopyable {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    // ウィンドウメッセージ処理
    bool ProcessMessages();

    // アクセサ
    HWND GetHandle() const { return hwnd_; }
    uint32 GetWidth() const { return width_; }
    uint32 GetHeight() const { return height_; }
    bool IsFullscreen() const { return fullscreen_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    void RegisterWindowClass();
    void CreateWindowInstance(const WindowConfig& config);

private:
    HWND hwnd_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    uint32 width_ = 0;
    uint32 height_ = 0;
    bool fullscreen_ = false;
    std::wstring className_;
};

} // namespace UnoEngine
