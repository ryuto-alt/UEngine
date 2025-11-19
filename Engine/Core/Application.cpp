#include "Application.h"
#include "../Resource/ResourceLoader.h"
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

    // 入力システム初期化
    input_ = MakeUnique<InputManager>();
    window_->SetMessageCallback([this](UINT msg, WPARAM wparam, LPARAM lparam) {
        input_->ProcessMessage(msg, wparam, lparam);

        // ウィンドウリサイズ処理
        if (msg == WM_SIZE) {
            uint32 width = LOWORD(lparam);
            uint32 height = HIWORD(lparam);
            if (width > 0 && height > 0) {
                graphics_->OnResize(width, height);
            }
        }
    });
    
    // 描画システム初期化
    sceneManager_ = MakeUnique<SceneManager>();
    sceneManager_->SetApplication(this);
    renderSystem_ = MakeUnique<RenderSystem>();
    lightManager_ = MakeUnique<LightManager>();
    renderer_ = MakeUnique<Renderer>();
    renderer_->Initialize(graphics_.get(), window_.get());

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

        // 入力更新
        input_->Update();

        // 更新
        sceneManager_->Update(deltaTime);
        OnUpdate(deltaTime);

        // 描画
        OnRender();
    }
}

void Application::OnRender() {
    graphics_->BeginFrame();
    
    Scene* scene = sceneManager_->GetActiveScene();
    if (scene) {
        RenderView view;
        scene->OnRender(view);
        
        auto items = renderSystem_->CollectRenderables(scene, view);

        renderer_->Draw(view, items, lightManager_.get(), scene);
    }
    
    graphics_->EndFrame();
    graphics_->Present();
}

void Application::Shutdown() {
    OnShutdown();
    input_.reset();
    graphics_.reset();
    window_.reset();
}

} // namespace UnoEngine
