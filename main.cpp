#include "Engine/Core/Application.h"
#include "Engine/Core/Camera.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Graphics/Pipeline.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/ObjLoader.h"
#include "Engine/Graphics/ConstantBuffer.h"
#include "Engine/Graphics/Texture2D.h"
#include "Engine/Graphics/Sprite.h"
#include "Engine/Graphics/SpritePipeline.h"
#include "Engine/Graphics/DirectionalLight.h"
#include "Engine/UI/ImGuiManager.h"
#include <imgui.h>
#include "Engine/Math/Math.h"
#include "Engine/Input/InputManager.h"

using namespace UnoEngine;

struct alignas(256) TransformCB {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4X4 mvp;
};

struct alignas(256) LightCB {
    DirectX::XMFLOAT3 directionalLightDirection;
    float padding0;
    DirectX::XMFLOAT3 directionalLightColor;
    float directionalLightIntensity;
    DirectX::XMFLOAT3 ambientLight;
    float padding1;
    DirectX::XMFLOAT3 cameraPosition;
    float padding2;
};

struct alignas(256) MaterialCB {
    DirectX::XMFLOAT3 albedo;
    float metallic;
    float roughness;
    DirectX::XMFLOAT3 padding;
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

        vertexShader_.CompileFromFile(L"Shaders/PBRVS.hlsl", ShaderStage::Vertex);
        pixelShader_.CompileFromFile(L"Shaders/PBRPS.hlsl", ShaderStage::Pixel);

        pipeline_.Initialize(device, vertexShader_, pixelShader_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

        spriteVertexShader_.CompileFromFile(L"Shaders/SpriteVS.hlsl", ShaderStage::Vertex);
        spritePixelShader_.CompileFromFile(L"Shaders/SpritePS.hlsl", ShaderStage::Pixel);

        spritePipeline_.Initialize(device, spriteVertexShader_, spritePixelShader_);

        // リソース作成用の一時コマンドアロケータとリスト
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> initAllocator;
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&initAllocator));
        commandList->Reset(initAllocator.Get(), nullptr);

        mesh_ = ObjLoader::Load(GetGraphics(), commandList, "resources/model/testmodel/testmodel.obj");

        if (!mesh_.HasMaterial()) {
            texture_.LoadFromFile(GetGraphics(), commandList, L"resources/tex/uvChecker.png", 0);
        }

        // sprite_.Initialize(GetGraphics());
        // sprite_.LoadTexture(GetGraphics(), commandList, L"resources/tex/uvChecker.png", 1);
        // sprite_.SetPosition(100, 100);
        // sprite_.SetScale(1.0f);

        // コマンドリストを実行してGPU処理完了を待つ
        commandList->Close();
        ID3D12CommandList* commandLists[] = { commandList };
        commandQueue->ExecuteCommandLists(1, commandLists);

        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
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
        lightBuffer_.Create(device);
        materialBuffer_.Create(device);

        // ディレクショナルライトの設定（真上から照らす）
        directionalLight_.SetDirection(Vector3(0.0f, -1.0f, 0.0f));
        directionalLight_.SetColor(Vector3(1.0f, 1.0f, 1.0f));
        directionalLight_.SetIntensity(2.0f);

        // ImGui初期化（SRVインデックス2を使用）
        imguiManager_.Initialize(GetGraphics(), GetWindow(), 2);
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
        
        // SRVヒープ設定
        ID3D12DescriptorHeap* heaps[] = { GetGraphics()->GetSRVHeap() };
        cmdList->SetDescriptorHeaps(1, heaps);
        
        // テクスチャ設定
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = GetGraphics()->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
        uint32 srvIndex = 0;

        if (mesh_.HasMaterial() && mesh_.GetMaterial()->HasDiffuseTexture()) {
            srvIndex = mesh_.GetMaterial()->GetSRVIndex();
        } else {
            srvIndex = texture_.GetSRVIndex();
        }

        srvHandle.ptr += srvIndex * GetGraphics()->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);

        // MVP行列計算
        Matrix4x4 world = Matrix4x4::RotationY(rotation_);
        Matrix4x4 view = camera_.GetViewMatrix();
        Matrix4x4 projection = camera_.GetProjectionMatrix();
        Matrix4x4 mvp = world * view * projection;

        // Transform定数バッファ更新
        TransformCB transformData;
        DirectX::XMStoreFloat4x4(&transformData.world, DirectX::XMMatrixTranspose(world.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.view, DirectX::XMMatrixTranspose(view.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.projection, DirectX::XMMatrixTranspose(projection.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.mvp, DirectX::XMMatrixTranspose(mvp.GetXMMatrix()));
        constantBuffer_.Update(transformData);

        // Light定数バッファ更新
        LightCB lightData;
        Vector3 lightDir = directionalLight_.GetDirection();
        Vector3 lightColor = directionalLight_.GetColor();
        Vector3 cameraPos = camera_.GetPosition();
        lightData.directionalLightDirection = DirectX::XMFLOAT3(lightDir.GetX(), lightDir.GetY(), lightDir.GetZ());
        lightData.directionalLightColor = DirectX::XMFLOAT3(lightColor.GetX(), lightColor.GetY(), lightColor.GetZ());
        lightData.directionalLightIntensity = directionalLight_.GetIntensity();
        lightData.ambientLight = DirectX::XMFLOAT3(0.03f, 0.03f, 0.03f);
        lightData.cameraPosition = DirectX::XMFLOAT3(cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());
        lightBuffer_.Update(lightData);

        // Material定数バッファ更新
        MaterialCB materialData;
        materialData.albedo = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
        materialData.metallic = 0.0f;
        materialData.roughness = 0.5f;
        materialBuffer_.Update(materialData);

        // 定数バッファ設定
        cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress());
        cmdList->SetGraphicsRootConstantBufferView(2, lightBuffer_.GetGPUAddress());
        cmdList->SetGraphicsRootConstantBufferView(3, materialBuffer_.GetGPUAddress());

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        auto vbView = mesh_.GetVertexBuffer().GetView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);

        auto ibView = mesh_.GetIndexBuffer().GetView();
        cmdList->IASetIndexBuffer(&ibView);

        cmdList->DrawIndexedInstanced(mesh_.GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);

        // ImGui描画
        imguiManager_.BeginFrame();
        
        // ImGuiデモウィンドウ表示
        ImGui::ShowDemoWindow();
        
        imguiManager_.EndFrame();
        imguiManager_.Render(cmdList);

        // sprite_.Draw(GetGraphics(), cmdList, spritePipeline_.GetPipelineState(), spritePipeline_.GetRootSignature());
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
    ConstantBuffer<LightCB> lightBuffer_;
    ConstantBuffer<MaterialCB> materialBuffer_;
    Texture2D texture_;
    float rotation_ = 0.0f;

    Shader spriteVertexShader_;
    Shader spritePixelShader_;
    SpritePipeline spritePipeline_;
    Sprite sprite_;
    ImGuiManager imguiManager_;

    DirectionalLight directionalLight_;
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
