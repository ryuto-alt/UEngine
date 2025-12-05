#include "pch.h"
#include "Game/GameApplication.h"
#include "Engine/Core/Scene.h"
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

        // Load Scene
        auto scene = MakeUnique<Scene>();
        GetSceneManager()->LoadScene(std::move(scene));

        // Set close request callback for save confirmation
        GetWindow()->SetCloseRequestCallback([this]() -> bool {
            auto* scene = GetSceneManager()->GetActiveScene();
            auto* editorUI = scene ? scene->GetEditorUI() : nullptr;

            // 変更がない場合はそのまま閉じる
            if (!editorUI || !editorUI->IsDirty()) {
                return true;
            }

            int result = MessageBoxW(
                GetWindow()->GetHandle(),
                L"変更を保存しますか？",
                L"UnoEngine",
                MB_YESNOCANCEL | MB_ICONQUESTION
            );

            if (result == IDYES) {
                editorUI->SaveScene("assets/scenes/default_scene.json");
                return true;
            } else if (result == IDNO) {
                return true;
            } else {
                return false; // Cancel
            }
        });
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
