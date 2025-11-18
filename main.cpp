#include "Engine/Core/Application.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/SceneManager.h"
#include "Game/Scenes/GameScene.h"
#include "Engine/Graphics/Shader.h"
#include "Engine/Graphics/Pipeline.h"
#include "Engine/Graphics/ConstantBuffer.h"
#include "Engine/Graphics/SpritePipeline.h"
#include "Engine/Graphics/MeshRenderer.h"
#include "Engine/Graphics/DirectionalLightComponent.h"
#include "Engine/Graphics/ResourceLoader.h"
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
        config.window.title = L"UnoEngine - Game Scene";
        config.window.width = 1280;
        config.window.height = 720;
        config.graphics.enableDebugLayer = true;
        return config;
    }

protected:
    void OnInit() override {
        auto* device = GetGraphics()->GetDevice();

        // Compile shaders
        vertexShader_.CompileFromFile(L"Shaders/PBRVS.hlsl", ShaderStage::Vertex);
        pixelShader_.CompileFromFile(L"Shaders/PBRPS.hlsl", ShaderStage::Pixel);
        pipeline_.Initialize(device, vertexShader_, pixelShader_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

        spriteVertexShader_.CompileFromFile(L"Shaders/SpriteVS.hlsl", ShaderStage::Vertex);
        spritePixelShader_.CompileFromFile(L"Shaders/SpritePS.hlsl", ShaderStage::Pixel);
        spritePipeline_.Initialize(device, spriteVertexShader_, spritePixelShader_);

        // Setup camera
        camera_.SetPosition(Vector3(0.0f, 0.0f, -5.0f));
        camera_.SetPerspective(
            Math::ToRadians(60.0f),
            static_cast<float>(GetWindow()->GetWidth()) / GetWindow()->GetHeight(),
            0.1f,
            100.0f
        );

        // Create constant buffers
        constantBuffer_.Create(device);
        lightBuffer_.Create(device);
        materialBuffer_.Create(device);

        // ImGui initialization
        imguiManager_.Initialize(GetGraphics(), GetWindow(), 2);

        // Initialize ResourceLoader
        ResourceLoader::Initialize(GetGraphics());

        // Setup SceneManager services
        auto& sceneManager = SceneManager::GetInstance();
        sceneManager.SetCamera(&camera_);
        sceneManager.SetInputManager(GetInput());

        // Register and load GameScene
        sceneManager.RegisterScene<GameScene>("GameScene");
        sceneManager.LoadScene("GameScene");
    }

    void OnUpdate(float deltaTime) override {
        auto* input = GetInput();
        const auto& keyboard = input->GetKeyboard();

        // Escape to quit
        if (keyboard.IsPressed(KeyCode::Escape)) {
            PostQuitMessage(0);
        }

        // Update scene
        SceneManager::GetInstance().OnUpdate(deltaTime);

        // Rotate model for demo
        rotation_ += deltaTime;
    }

    void OnRender() override {
        auto* cmdList = GetGraphics()->GetCommandList();
        auto* scene = SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        // Viewport and scissor rect
        D3D12_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(GetWindow()->GetWidth());
        viewport.Height = static_cast<float>(GetWindow()->GetHeight());
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissorRect = {};
        scissorRect.right = GetWindow()->GetWidth();
        scissorRect.bottom = GetWindow()->GetHeight();

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissorRect);

        // Pipeline setup
        cmdList->SetPipelineState(pipeline_.GetPipelineState());
        cmdList->SetGraphicsRootSignature(pipeline_.GetRootSignature());
        
        ID3D12DescriptorHeap* heaps[] = { GetGraphics()->GetSRVHeap() };
        cmdList->SetDescriptorHeaps(1, heaps);
        
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Find directional light
        DirectionalLightComponent* lightComp = nullptr;
        for (const auto& obj : scene->GetGameObjects()) {
            lightComp = obj->GetComponent<DirectionalLightComponent>();
            if (lightComp) break;
        }

        // Render all MeshRenderers
        for (const auto& obj : scene->GetGameObjects()) {
            auto* meshRenderer = obj->GetComponent<MeshRenderer>();
            if (!meshRenderer || !meshRenderer->GetMesh()) continue;

            auto* mesh = meshRenderer->GetMesh();
            auto* material = meshRenderer->GetMaterial();

            // Texture setup
            D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = GetGraphics()->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
            uint32 srvIndex = 0;

            if (material && material->HasDiffuseTexture()) {
                srvIndex = material->GetSRVIndex();
            }

            srvHandle.ptr += srvIndex * GetGraphics()->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);

            // Transform matrices
            Matrix4x4 world = Matrix4x4::RotationY(rotation_) * obj->GetTransform().GetWorldMatrix();
            Matrix4x4 view = camera_.GetViewMatrix();
            Matrix4x4 projection = camera_.GetProjectionMatrix();
            Matrix4x4 mvp = world * view * projection;

            TransformCB transformData;
            DirectX::XMStoreFloat4x4(&transformData.world, DirectX::XMMatrixTranspose(world.GetXMMatrix()));
            DirectX::XMStoreFloat4x4(&transformData.view, DirectX::XMMatrixTranspose(view.GetXMMatrix()));
            DirectX::XMStoreFloat4x4(&transformData.projection, DirectX::XMMatrixTranspose(projection.GetXMMatrix()));
            DirectX::XMStoreFloat4x4(&transformData.mvp, DirectX::XMMatrixTranspose(mvp.GetXMMatrix()));
            constantBuffer_.Update(transformData);

            // Light data
            LightCB lightData;
            if (lightComp) {
                Vector3 lightDir = lightComp->GetDirection();
                Vector3 lightColor = lightComp->GetColor();
                lightData.directionalLightDirection = DirectX::XMFLOAT3(lightDir.GetX(), lightDir.GetY(), lightDir.GetZ());
                lightData.directionalLightColor = DirectX::XMFLOAT3(lightColor.GetX(), lightColor.GetY(), lightColor.GetZ());
                lightData.directionalLightIntensity = lightComp->GetIntensity();
            }
            lightData.ambientLight = DirectX::XMFLOAT3(0.03f, 0.03f, 0.03f);
            Vector3 cameraPos = camera_.GetPosition();
            lightData.cameraPosition = DirectX::XMFLOAT3(cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());
            lightBuffer_.Update(lightData);

            // Material data
            MaterialCB materialData;
            materialData.albedo = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
            materialData.metallic = 0.0f;
            materialData.roughness = 0.5f;
            materialBuffer_.Update(materialData);

            // Set constant buffers
            cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress());
            cmdList->SetGraphicsRootConstantBufferView(2, lightBuffer_.GetGPUAddress());
            cmdList->SetGraphicsRootConstantBufferView(3, materialBuffer_.GetGPUAddress());

            // Draw mesh
            auto vbView = mesh->GetVertexBuffer().GetView();
            cmdList->IASetVertexBuffers(0, 1, &vbView);

            auto ibView = mesh->GetIndexBuffer().GetView();
            cmdList->IASetIndexBuffer(&ibView);

            cmdList->DrawIndexedInstanced(mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
        }

        // ImGui
        imguiManager_.BeginFrame();
        ImGui::ShowDemoWindow();
        imguiManager_.EndFrame();
        imguiManager_.Render(cmdList);
    }

    void OnShutdown() override {
        ResourceLoader::Shutdown();
    }

private:
    Shader vertexShader_;
    Shader pixelShader_;
    Pipeline pipeline_;
    Camera camera_;
    ConstantBuffer<TransformCB> constantBuffer_;
    ConstantBuffer<LightCB> lightBuffer_;
    ConstantBuffer<MaterialCB> materialBuffer_;
    float rotation_ = 0.0f;

    Shader spriteVertexShader_;
    Shader spritePixelShader_;
    SpritePipeline spritePipeline_;
    ImGuiManager imguiManager_;
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
