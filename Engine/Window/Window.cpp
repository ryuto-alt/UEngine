#include "Window.h"
#include <stdexcept>

// ImGui Win32ハンドラの前方宣言
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace UnoEngine {

Window::Window(const WindowConfig& config)
    : hinstance_(GetModuleHandle(nullptr))
    , width_(config.width)
    , height_(config.height)
    , fullscreen_(config.fullscreen)
    , className_(L"UnoEngineWindowClass") {

    RegisterWindowClass();
    CreateWindowInstance(config);
}

Window::~Window() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
    UnregisterClassW(className_.c_str(), hinstance_);
}

void Window::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className_.c_str();

    if (!RegisterClassExW(&wc)) {
        throw std::runtime_error("Failed to register window class");
    }
}

void Window::CreateWindowInstance(const WindowConfig& config) {
    DWORD style = fullscreen_ ? WS_POPUP : WS_OVERLAPPEDWINDOW;

    // クライアント領域が指定サイズになるよう調整
    RECT rect = { 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
    AdjustWindowRect(&rect, style, FALSE);

    hwnd_ = CreateWindowExW(
        0,
        className_.c_str(),
        config.title.c_str(),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hinstance_,
        this  // lpParamにthisを渡す
    );

    if (!hwnd_) {
        throw std::runtime_error("Failed to create window");
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
}

bool Window::ProcessMessages() {
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // ImGui入力処理（最優先）
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    Window* window = nullptr;

    if (msg == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<Window*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    // コールバック呼び出し
    if (window && window->messageCallback_) {
        window->messageCallback_(msg, wparam, lparam);
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (window) {
            window->width_ = LOWORD(lparam);
            window->height_ = HIWORD(lparam);
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace UnoEngine
