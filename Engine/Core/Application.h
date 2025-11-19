#pragma once

#include "Types.h"
#include "NonCopyable.h"
#include "RenderSystem.h"
#include "SceneManager.h"
#include "../Window/Window.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/LightManager.h"
#include "../Input/InputManager.h"
#include "../../Game/Renderer.h"

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

    // アクセサ
    Window* GetWindow() const { return window_.get(); }
    GraphicsDevice* GetGraphics() const { return graphics_.get(); }
    InputManager* GetInput() const { return input_.get(); }
    SceneManager* GetSceneManager() const { return sceneManager_.get(); }

protected:
    // オーバーライド可能なライフサイクル
    virtual void OnInit() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnShutdown() {}

private:
    void Initialize();
    void MainLoop();
    virtual void OnRender() final;
    void Shutdown();

private:
    ApplicationConfig config_;
    UniquePtr<Window> window_;
    UniquePtr<GraphicsDevice> graphics_;
    UniquePtr<InputManager> input_;
    UniquePtr<SceneManager> sceneManager_;
    
    UniquePtr<RenderSystem> renderSystem_;
    UniquePtr<LightManager> lightManager_;
    UniquePtr<Renderer> renderer_;
    
    bool running_ = false;
};

} // namespace UnoEngine
