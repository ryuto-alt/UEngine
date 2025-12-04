#pragma once

#include "Types.h"
#include "NonCopyable.h"
#include "../Rendering/RenderSystem.h"
#include "SceneManager.h"
#include "../Window/Window.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Rendering/LightManager.h"
#include "../Input/InputManager.h"
#include "../Rendering/Renderer.h"
#include "../Systems/SystemManager.h"
#include "../Particle/ParticleSystem.h"
#include "../Editor/ParticleEditor.h"

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
    SystemManager* GetSystemManager() { return &systemManager_; }
    ParticleSystem* GetParticleSystem() const { return particleSystem_.get(); }
    ParticleEditor* GetParticleEditor() const { return particleEditor_.get(); }

protected:
    // オーバーライド可能なライフサイクル
    virtual void OnInit() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnShutdown() {}

    // サブクラスからアクセス可能なメンバー
    UniquePtr<GraphicsDevice> graphics_;
    UniquePtr<RenderSystem> renderSystem_;
    UniquePtr<LightManager> lightManager_;
    UniquePtr<Renderer> renderer_;
    UniquePtr<ParticleSystem> particleSystem_;
    UniquePtr<ParticleEditor> particleEditor_;
    SystemManager systemManager_;

private:
    void Initialize();
    void MainLoop();
    virtual void OnRender();
    void Shutdown();

private:
    ApplicationConfig config_;
    UniquePtr<Window> window_;
    UniquePtr<InputManager> input_;
    UniquePtr<SceneManager> sceneManager_;
    
    bool running_ = false;
};

} // namespace UnoEngine
