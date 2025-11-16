#include "Engine/Core/Application.h"

using namespace UnoEngine;

class SampleApp : public Application {
public:
    SampleApp() : Application(CreateConfig()) {}

private:
    static ApplicationConfig CreateConfig() {
        ApplicationConfig config;
        config.window.title = L"UnoEngine - Sample";
        config.window.width = 1280;
        config.window.height = 720;
        config.graphics.enableDebugLayer = true;
        return config;
    }

protected:
    void OnInit() override {
        // 初期化処理
    }

    void OnUpdate(float deltaTime) override {
        // 更新処理
    }

    void OnRender() override {
        // 描画処理（現在はクリアのみ）
    }

    void OnShutdown() override {
        // 終了処理
    }
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
