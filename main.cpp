#include "Game/GameApplication.h"
#include "Game/Scenes/GameScene.h"
#include "Engine/Resource/ResourceLoader.h"
#include "Engine/Input/InputManager.h"

using namespace UnoEngine;

class SampleApp : public GameApplication {
public:
    SampleApp() : GameApplication(CreateConfig()) {}

private:
    static ApplicationConfig CreateConfig() {
        ApplicationConfig config;
        config.window.title = L"UnoEngine - Game Scene";
        config.window.width = 1280;
        config.window.height = 720;
        config.graphics.enableDebugLayer = true;
        return config;
    }

protected:
    void OnInit() override {
        GameApplication::OnInit();

        // Initialize ResourceLoader
        ResourceLoader::Initialize(GetGraphics());

        // Load GameScene
        auto gameScene = MakeUnique<GameScene>();
        GetSceneManager()->LoadScene(std::move(gameScene));
    }

    void OnUpdate(float deltaTime) override {
        auto* input = GetInput();
        const auto& keyboard = input->GetKeyboard();

        if (keyboard.IsPressed(KeyCode::Escape)) {
            PostQuitMessage(0);
        }
    }

    // OnRenderは削除 (Application::OnRender()がfinalで自動実行)

    void OnShutdown() override {
        ResourceLoader::Shutdown();
    }

private:
    // 全ての描画リソースはRenderer/Application側に移動
};

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
) {
    SampleApp app;
    return app.Run();
}
