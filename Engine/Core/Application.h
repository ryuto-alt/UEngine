#pragma once

#include "Types.h"
#include "NonCopyable.h"
#include "../Window/Window.h"
#include "../Graphics/GraphicsDevice.h"

namespace UnoEngine {

// アプリケーション設定
struct ApplicationConfig {
    WindowConfig window;
    GraphicsConfig graphics;
};

// エンジンアプリケーション基底クラス
class Application : public NonCopyable {
public:
    explicit Application(const ApplicationConfig& config = {});
    virtual ~Application() = default;

    // エンジン実行
    int Run();

protected:
    // オーバーライド可能なライフサイクル
    virtual void OnInit() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnShutdown() {}

    // アクセサ
    Window* GetWindow() const { return window_.get(); }
    GraphicsDevice* GetGraphics() const { return graphics_.get(); }

private:
    void Initialize();
    void MainLoop();
    void Shutdown();

private:
    ApplicationConfig config_;
    UniquePtr<Window> window_;
    UniquePtr<GraphicsDevice> graphics_;
    bool running_ = false;
};

} // namespace UnoEngine
