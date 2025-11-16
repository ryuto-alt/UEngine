#include "Engine/Core/Application.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Graphics/Pipeline.h"
#include "Engine/Graphics/VertexBuffer.h"

using namespace UnoEngine;

// 頂点構造体
struct Vertex {
    float position[3];
    float color[4];
};

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
        auto* device = GetGraphics()->GetDevice();

        // シェーダーコンパイル
        vertexShader_.CompileFromFile(L"Shaders/BasicVS.hlsl", ShaderStage::Vertex);
        pixelShader_.CompileFromFile(L"Shaders/BasicPS.hlsl", ShaderStage::Pixel);

        // パイプライン作成
        pipeline_.Initialize(device, vertexShader_, pixelShader_, DXGI_FORMAT_R8G8B8A8_UNORM);

        // 三角形の頂点データ
        Vertex triangleVertices[] = {
            { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },   // 上: 赤
            { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // 右下: 緑
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }  // 左下: 青
        };

        vertexBuffer_.Create(device, triangleVertices, sizeof(triangleVertices), sizeof(Vertex));
    }

    void OnUpdate(float deltaTime) override {
        // 更新処理
    }

    void OnRender() override {
        auto* cmdList = GetGraphics()->GetCommandList();

        // ビューポートとシザー矩形設定
        D3D12_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(GetWindow()->GetWidth());
        viewport.Height = static_cast<float>(GetWindow()->GetHeight());
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissorRect = {};
        scissorRect.right = GetWindow()->GetWidth();
        scissorRect.bottom = GetWindow()->GetHeight();

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissorRect);

        // パイプライン設定
        cmdList->SetPipelineState(pipeline_.GetPipelineState());
        cmdList->SetGraphicsRootSignature(pipeline_.GetRootSignature());

        // 頂点バッファ設定
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        auto vbView = vertexBuffer_.GetView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);

        // 描画
        cmdList->DrawInstanced(3, 1, 0, 0);
    }

    void OnShutdown() override {
        // 終了処理
    }

private:
    Shader vertexShader_;
    Shader pixelShader_;
    Pipeline pipeline_;
    VertexBuffer vertexBuffer_;
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
