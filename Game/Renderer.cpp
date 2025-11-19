#include "Renderer.h"
#include "../Engine/Graphics/DirectionalLightComponent.h"
#include "../Engine/Graphics/Shader.h"
#include <imgui.h>
#include <Windows.h>

namespace UnoEngine {

void Renderer::Initialize(GraphicsDevice* graphics, Window* window) {
    graphics_ = graphics;
    window_ = window;

    auto* device = graphics_->GetDevice();

    // Create shaders
    Shader vertexShader;
    vertexShader.CompileFromFile(L"Shaders/PBRVS.hlsl", ShaderStage::Vertex);

    Shader pixelShader;
    pixelShader.CompileFromFile(L"Shaders/PBRPS.hlsl", ShaderStage::Pixel);

    // Create pipeline
    pipeline_.Initialize(device, vertexShader, pixelShader, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    // Create constant buffers
    constantBuffer_.Create(device);
    lightBuffer_.Create(device);
    materialBuffer_.Create(device);

    // Initialize ImGui
    imguiManager_ = MakeUnique<ImGuiManager>();
    imguiManager_->Initialize(graphics_, window_, 2);
}

void Renderer::Draw(const RenderView& view, const std::vector<RenderItem>& renderItems, LightManager* lightManager) {
    if (!view.camera) return;

    auto* cmdList = graphics_->GetCommandList();

    SetupViewport();

    // Get directional light
    auto* lightComp = lightManager ? lightManager->GetDirectionalLight() : nullptr;

    // Render all items
    for (const auto& item : renderItems) {
        if (!item.mesh || !item.material) continue;

        // Setup pipeline state for each draw call
        cmdList->SetPipelineState(pipeline_.GetPipelineState());
        cmdList->SetGraphicsRootSignature(pipeline_.GetRootSignature());
        
        ID3D12DescriptorHeap* heaps[] = { graphics_->GetSRVHeap() };
        cmdList->SetDescriptorHeaps(1, heaps);
        
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Update and set TransformCB
        Matrix4x4 viewMatrix = view.camera->GetViewMatrix();
        Matrix4x4 projection = view.camera->GetProjectionMatrix();
        Matrix4x4 mvp = item.worldMatrix * viewMatrix * projection;

        TransformCB transformData;
        DirectX::XMStoreFloat4x4(&transformData.world, DirectX::XMMatrixTranspose(item.worldMatrix.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.view, DirectX::XMMatrixTranspose(viewMatrix.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.projection, DirectX::XMMatrixTranspose(projection.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.mvp, DirectX::XMMatrixTranspose(mvp.GetXMMatrix()));
        constantBuffer_.Update(transformData);
        cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress());

        // Set SRV (texture) - デバッグ出力強化
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = graphics_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
        uint32 srvIndex = 0;
        bool hasTexture = item.material->HasDiffuseTexture();
        
        char debugMsg[512];
        //sprintf_s(debugMsg, "Frame: HasTexture=%d, SRVIndex=%u, Material=%p\n", hasTexture ? 1 : 0, item.material->GetSRVIndex(), item.material);
        //OutputDebugStringA(debugMsg);
        
        if (hasTexture) {
            srvIndex = item.material->GetSRVIndex();
        }

        srvHandle.ptr += srvIndex * graphics_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);

        // Update and set LightCB
        LightCB lightData = {}; // ゼロ初期化
        // デフォルト値を設定
        lightData.directionalLightDirection = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
        lightData.directionalLightColor = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
        lightData.directionalLightIntensity = 1.0f;
        if (lightComp) {
            Vector3 lightDir = lightComp->GetDirection();
            Vector3 lightColor = lightComp->GetColor();
            lightData.directionalLightDirection = DirectX::XMFLOAT3(lightDir.GetX(), lightDir.GetY(), lightDir.GetZ());
            lightData.directionalLightColor = DirectX::XMFLOAT3(lightColor.GetX(), lightColor.GetY(), lightColor.GetZ());
            lightData.directionalLightIntensity = lightComp->GetIntensity();
            
            // デバッグ出力
            static int frameCount = 0;
            if (frameCount < 1) {
                char debugMsg[512];
                sprintf_s(debugMsg, "LightData: dir=(%.2f,%.2f,%.2f), color=(%.2f,%.2f,%.2f), intensity=%.2f\n",
                         lightDir.GetX(), lightDir.GetY(), lightDir.GetZ(),
                         lightColor.GetX(), lightColor.GetY(), lightColor.GetZ(),
                         lightComp->GetIntensity());
                OutputDebugStringA(debugMsg);
                frameCount++;
            }
        }
        lightData.ambientLight = DirectX::XMFLOAT3(0.3f, 0.3f, 0.3f);
        Vector3 cameraPos = view.camera->GetPosition();
        lightData.cameraPosition = DirectX::XMFLOAT3(cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());
        lightBuffer_.Update(lightData);
        cmdList->SetGraphicsRootConstantBufferView(2, lightBuffer_.GetGPUAddress());

        // Update and set MaterialCB
        MaterialCB materialData;
        const auto& matData = item.material->GetData();
        materialData.albedo = DirectX::XMFLOAT3(matData.albedo[0], matData.albedo[1], matData.albedo[2]);
        materialData.metallic = matData.metallic;
        materialData.roughness = matData.roughness;
        
        // Debug output (first frame only)
        static bool debugPrinted = false;
        if (!debugPrinted) {
            char debugMsg[256];
            sprintf_s(debugMsg, "Renderer MaterialCB: albedo=(%.2f, %.2f, %.2f), metallic=%.2f, roughness=%.2f\n",
                     materialData.albedo.x, materialData.albedo.y, materialData.albedo.z,
                     materialData.metallic, materialData.roughness);
            OutputDebugStringA(debugMsg);
            debugPrinted = true;
        }
        
        materialBuffer_.Update(materialData);
        cmdList->SetGraphicsRootConstantBufferView(3, materialBuffer_.GetGPUAddress());

        // Draw mesh
        auto vbView = item.mesh->GetVertexBuffer().GetView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);

        auto ibView = item.mesh->GetIndexBuffer().GetView();
        cmdList->IASetIndexBuffer(&ibView);

        cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
    }

    RenderUI();
}

void Renderer::SetupViewport() {
    auto* cmdList = graphics_->GetCommandList();

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(window_->GetWidth());
    viewport.Height = static_cast<float>(window_->GetHeight());
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.right = window_->GetWidth();
    scissorRect.bottom = window_->GetHeight();

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);
}

void Renderer::RenderUI() {
    auto* cmdList = graphics_->GetCommandList();

    imguiManager_->BeginFrame();
    ImGui::ShowDemoWindow();
    imguiManager_->EndFrame();
    imguiManager_->Render(cmdList);
}

} // namespace UnoEngine
