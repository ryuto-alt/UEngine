#include "Application.h"
#include <chrono>

namespace UnoEngine {

Application::Application(const ApplicationConfig& config)
    : config_(config) {
}

int Application::Run() {
    try {
        Initialize();
        MainLoop();
        Shutdown();
        return 0;
    }
    catch (const std::exception& e) {
        // エラーハンドリング（将来的にはログシステムへ）
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}

void Application::Initialize() {
    window_ = MakeUnique<Window>(config_.window);
    graphics_ = MakeUnique<GraphicsDevice>(config_.graphics);
    graphics_->Initialize(window_.get());

    OnInit();
    running_ = true;
}

void Application::MainLoop() {
    using namespace std::chrono;
    auto lastTime = high_resolution_clock::now();

    while (running_) {
        // メッセージ処理
        if (!window_->ProcessMessages()) {
            running_ = false;
            break;
        }

        // デルタタイム計算
        auto currentTime = high_resolution_clock::now();
        float deltaTime = duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // 更新
        OnUpdate(deltaTime);

        // 描画
        graphics_->BeginFrame();
        OnRender();
        graphics_->EndFrame();
        graphics_->Present();
    }
}

void Application::Shutdown() {
    OnShutdown();
    graphics_.reset();
    window_.reset();
}

} // namespace UnoEngine
