// UnoGame - Editor無しのゲーム専用エントリーポイント
// エクスポート機能で生成されるGame.exeのソース

#include "pch.h"
#include "GameApplication.h"
#include "../Engine/Core/Scene.h"
#include "../Engine/Scene/SceneSerializer.h"
#include "../Engine/Input/InputManager.h"

using namespace UnoEngine;

class GameApp : public GameApplication {
public:
    GameApp() : GameApplication(CreateConfig()) {}

private:
    static ApplicationConfig CreateConfig() {
        ApplicationConfig config;
        config.window.title = L"UnoEngine Game";
        config.window.width = 1280;
        config.window.height = 720;
        config.graphics.enableDebugLayer = false;
        return config;
    }

    void OnInit() override {
        GameApplication::OnInit();

        // デフォルトシーンをロード
        auto scene = MakeUnique<Scene>();
        std::vector<std::unique_ptr<GameObject>> loadedObjects;

        if (SceneSerializer::LoadScene("assets/scenes/default.scene", loadedObjects)) {
            auto& sceneObjects = scene->GetGameObjects();
            for (auto& obj : loadedObjects) {
                sceneObjects.push_back(std::move(obj));
            }
        }

        GetSceneManager()->LoadScene(std::move(scene));
    }

    void OnUpdate(float deltaTime) override {
        (void)deltaTime;

        auto* input = GetInput();
        const auto& keyboard = input->GetKeyboard();

        if (keyboard.IsPressed(KeyCode::Escape)) {
            PostQuitMessage(0);
        }
    }
};

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    GameApp app;
    return app.Run();
}
