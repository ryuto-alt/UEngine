#include "Engine/Core/Application.h"
#include "Engine/Core/Camera.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Graphics/Pipeline.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/ObjLoader.h"
#include "Engine/Graphics/ConstantBuffer.h"
#include "Engine/Math/Math.h"
#include "Engine/Input/InputManager.h"

using namespace UnoEngine;

struct alignas(256) TransformCB {
    DirectX::XMFLOAT4X4 mvp;
};

class SampleApp : public Application {
public:
    SampleApp() : Application(CreateConfig()) {}

private:
    static ApplicationConfig CreateConfig() {
        ApplicationConfig config;
        config.window.title = L"UnoEngine - OBJ Viewer";
        config.window.width = 1280;
        config.window.height = 720;
        config.graphics.enableDebugLayer = true;
        return config;
    }

protected:
    void OnInit() override {
        auto* device = GetGraphics()->GetDevice();
        auto* commandQueue = GetGraphics()->GetCommandQueue();
        auto* commandList = GetGraphics()->GetCommandList();

        vertexShader_.CompileFromFile(L"Shaders/BasicVS.hlsl", ShaderStage::Vertex);
        pixelShader_.CompileFromFile(L"Shaders/BasicPS.hlsl", ShaderStage::Pixel);

        pipeline_.Initialize(device, vertexShader_, pixelShader_, DXGI_FORMAT_R8G8B8A8_UNORM);

        // リソース作成用の一時コマンドアロケータとリスト
        ComPtr<ID3D12CommandAllocator> initAllocator;
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&initAllocator));
        commandList->Reset(initAllocator.Get(), nullptr);

        mesh_ = ObjLoader::Load(device, commandList, "resources/model/testmodel/testmodel.obj");

        // コマンドリストを実行してGPU処理完了を待つ
        commandList->Close();
        ID3D12CommandList* commandLists[] = { commandList };
        commandQueue->ExecuteCommandLists(1, commandLists);

        ComPtr<ID3D12Fence> fence;
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        commandQueue->Signal(fence.Get(), 1);
        fence->SetEventOnCompletion(1, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
        CloseHandle(fenceEvent);

        camera_.SetPosition(Vector3(0.0f, 0.0f, -5.0f));
        camera_.SetPerspective(
            Math::ToRadians(60.0f),
            static_cast<float>(GetWindow()->GetWidth()) / GetWindow()->GetHeight(),
            0.1f,
            100.0f
        );

        constantBuffer_.Create(device);
    }

    void OnUpdate(float deltaTime) override {
        auto* input = GetInput();
        const auto& keyboard = input->GetKeyboard();
        const auto& mouse = input->GetMouse();

        // カメラ移動速度
        const float moveSpeed = 5.0f * deltaTime;
        const float rotateSpeed = 2.0f * deltaTime;

        // WASD移動
        Vector3 cameraPos = camera_.GetPosition();
        Vector3 forward = camera_.GetForward();
        Vector3 right = camera_.GetRight();

        if (keyboard.IsDown(KeyCode::W)) {
            cameraPos = cameraPos + forward * moveSpeed;
        }
        if (keyboard.IsDown(KeyCode::S)) {
            cameraPos = cameraPos - forward * moveSpeed;
        }
        if (keyboard.IsDown(KeyCode::A)) {
            cameraPos = cameraPos - right * moveSpeed;
        }
        if (keyboard.IsDown(KeyCode::D)) {
            cameraPos = cameraPos + right * moveSpeed;
        }
        if (keyboard.IsDown(KeyCode::Q)) {
            cameraPos.SetY(cameraPos.GetY() - moveSpeed);
        }
        if (keyboard.IsDown(KeyCode::E)) {
            cameraPos.SetY(cameraPos.GetY() + moveSpeed);
        }

        camera_.SetPosition(cameraPos);

        // 矢印キーで回転
        Quaternion rotation = camera_.GetRotation();
        if (keyboard.IsDown(KeyCode::Left)) {
            rotation = Quaternion::RotationAxis(Vector3::UnitY(), rotateSpeed) * rotation;
        }
        if (keyboard.IsDown(KeyCode::Right)) {
            rotation = Quaternion::RotationAxis(Vector3::UnitY(), -rotateSpeed) * rotation;
        }
        if (keyboard.IsDown(KeyCode::Up)) {
            rotation = rotation * Quaternion::RotationAxis(Vector3::UnitX(), rotateSpeed);
        }
        if (keyboard.IsDown(KeyCode::Down)) {
            rotation = rotation * Quaternion::RotationAxis(Vector3::UnitX(), -rotateSpeed);
        }

        camera_.SetRotation(rotation);

        // Escapeで終了
        if (keyboard.IsPressed(KeyCode::Escape)) {
            PostQuitMessage(0);
        }

        // 三角形を回転
        rotation_ += deltaTime;
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

        // MVP行列計算
        Matrix4x4 model = Matrix4x4::RotationY(rotation_);
        Matrix4x4 view = camera_.GetViewMatrix();
        Matrix4x4 projection = camera_.GetProjectionMatrix();
        Matrix4x4 mvp = model * view * projection;

        // 定数バッファ更新
        TransformCB cbData;
        DirectX::XMStoreFloat4x4(&cbData.mvp, DirectX::XMMatrixTranspose(mvp.GetXMMatrix()));
        constantBuffer_.Update(cbData);

        // 定数バッファ設定
        cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress());

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        auto vbView = mesh_.GetVertexBuffer().GetView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);

        auto ibView = mesh_.GetIndexBuffer().GetView();
        cmdList->IASetIndexBuffer(&ibView);

        cmdList->DrawIndexedInstanced(mesh_.GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
    }

    void OnShutdown() override {
    }

private:
    Shader vertexShader_;
    Shader pixelShader_;
    Pipeline pipeline_;
    Mesh mesh_;
    Camera camera_;
    ConstantBuffer<TransformCB> constantBuffer_;
    float rotation_ = 0.0f;
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
