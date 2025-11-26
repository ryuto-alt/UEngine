#include "Renderer.h"
#include "../Core/Scene.h"
#include "../Graphics/DirectionalLightComponent.h"
#include "../Graphics/Shader.h"
#include <imgui.h>
#include <Windows.h>

namespace UnoEngine {

void Renderer::Initialize(GraphicsDevice* graphics, Window* window) {
    graphics_ = graphics;
    window_ = window;

    auto* device = graphics_->GetDevice();

    // PBR Pipeline
    Shader vertexShader;
    vertexShader.CompileFromFile(L"Shaders/PBRVS.hlsl", ShaderStage::Vertex);

    Shader pixelShader;
    pixelShader.CompileFromFile(L"Shaders/PBRPS.hlsl", ShaderStage::Pixel);

    pipeline_.Initialize(device, vertexShader, pixelShader, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    // Skinned Pipeline
    Shader skinnedVS;
    skinnedVS.CompileFromFile(L"Shaders/SkinnedVS.hlsl", ShaderStage::Vertex);

    Shader skinnedPS;
    skinnedPS.CompileFromFile(L"Shaders/SkinnedPS.hlsl", ShaderStage::Pixel);

    skinnedPipeline_.Initialize(device, skinnedVS, skinnedPS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    constantBuffer_.Create(device);
    lightBuffer_.Create(device);
    materialBuffer_.Create(device);
    boneBuffer_.Create(device);

    imguiManager_ = MakeUnique<ImGuiManager>();
    imguiManager_->Initialize(graphics_, window_, 2);
}

void Renderer::Draw(const RenderView& view, const std::vector<RenderItem>& items, LightManager* lights, Scene* scene) {
    if (!view.camera) return;

    SetupViewport();
    UpdateLighting(view, lights);
    RenderMeshes(view, items);
    RenderUI(scene);
}

void Renderer::UpdateLighting(const RenderView& view, LightManager* lights) {
    auto gpuLight = lights ? lights->BuildGPULightData() : GPULightData{};
    
    LightCB lightData;
    lightData.directionalLightDirection = DirectX::XMFLOAT3(gpuLight.direction.GetX(), gpuLight.direction.GetY(), gpuLight.direction.GetZ());
    lightData.directionalLightColor = DirectX::XMFLOAT3(gpuLight.color.GetX(), gpuLight.color.GetY(), gpuLight.color.GetZ());
    lightData.directionalLightIntensity = gpuLight.intensity;
    lightData.ambientLight = DirectX::XMFLOAT3(gpuLight.ambient.GetX(), gpuLight.ambient.GetY(), gpuLight.ambient.GetZ());
    
    auto cameraPos = view.camera->GetPosition();
    lightData.cameraPosition = DirectX::XMFLOAT3(cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());
    
    lightBuffer_.Update(lightData);
}

void Renderer::RenderMeshes(const RenderView& view, const std::vector<RenderItem>& items) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();
    
    cmdList->SetPipelineState(pipeline_.GetPipelineState());
    cmdList->SetGraphicsRootSignature(pipeline_.GetRootSignature());
    
    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    cmdList->SetGraphicsRootConstantBufferView(2, lightBuffer_.GetGPUAddress());
    
    auto viewMatrix = view.camera->GetViewMatrix();
    auto projection = view.camera->GetProjectionMatrix();
    
    for (const auto& item : items) {
        if (!item.mesh || !item.material) continue;
        
        TransformCB transformData;
        auto mvp = item.worldMatrix * viewMatrix * projection;
        DirectX::XMStoreFloat4x4(&transformData.world, DirectX::XMMatrixTranspose(item.worldMatrix.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.view, DirectX::XMMatrixTranspose(viewMatrix.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.projection, DirectX::XMMatrixTranspose(projection.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.mvp, DirectX::XMMatrixTranspose(mvp.GetXMMatrix()));
        constantBuffer_.Update(transformData);
        cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress());
        
        cmdList->SetGraphicsRootDescriptorTable(1, item.material->GetAlbedoSRV(heap));
        
        MaterialCB materialData;
        const auto& matData = item.material->GetData();
        materialData.albedo = DirectX::XMFLOAT3(matData.albedo[0], matData.albedo[1], matData.albedo[2]);
        materialData.metallic = matData.metallic;
        materialData.roughness = matData.roughness;
        materialBuffer_.Update(materialData);
        cmdList->SetGraphicsRootConstantBufferView(3, materialBuffer_.GetGPUAddress());
        
        auto vbView = item.mesh->GetVertexBuffer().GetView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);
        auto ibView = item.mesh->GetIndexBuffer().GetView();
        cmdList->IASetIndexBuffer(&ibView);
        cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
    }
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

void Renderer::RenderUI(Scene* scene) {
    auto* cmdList = graphics_->GetCommandList();

    imguiManager_->BeginFrame();

    if (scene) {
        scene->OnImGui();
    }

    imguiManager_->EndFrame();
    imguiManager_->Render(cmdList);
}

void Renderer::RenderUIOnly(Scene* scene) {
    SetupViewport();
    RenderUI(scene);
}

void Renderer::DrawToTexture(ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                             D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, const RenderView& view,
                             const std::vector<RenderItem>& items, LightManager* lightManager,
                             const std::vector<SkinnedRenderItem>& skinnedItems) {
    if (!view.camera) return;

    auto* cmdList = graphics_->GetCommandList();

    // Resource barrier: PIXEL_SHADER_RESOURCE -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTarget;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Set render target with depth buffer
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Clear render target and depth buffer
    const float clearColor[] = {0.2f, 0.3f, 0.4f, 1.0f};
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set viewport based on texture size
    D3D12_RESOURCE_DESC desc = renderTarget->GetDesc();
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(desc.Width);
    viewport.Height = static_cast<float>(desc.Height);
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.right = static_cast<LONG>(desc.Width);
    scissorRect.bottom = static_cast<LONG>(desc.Height);

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    // Render scene
    UpdateLighting(view, lightManager);
    RenderMeshes(view, items);

    // Render skinned meshes
    if (!skinnedItems.empty()) {
        RenderSkinnedMeshes(view, skinnedItems);
    }

    // Resource barrier: RENDER_TARGET -> PIXEL_SHADER_RESOURCE
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}

void Renderer::DrawSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items, LightManager* lights) {
    if (!view.camera) return;

    SetupViewport();
    UpdateLighting(view, lights);
    RenderSkinnedMeshes(view, items);
}

void Renderer::RenderSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();

    cmdList->SetPipelineState(skinnedPipeline_.GetPipelineState());
    cmdList->SetGraphicsRootSignature(skinnedPipeline_.GetRootSignature());

    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmdList->SetGraphicsRootConstantBufferView(2, lightBuffer_.GetGPUAddress());

    auto viewMatrix = view.camera->GetViewMatrix();
    auto projection = view.camera->GetProjectionMatrix();

    for (const auto& item : items) {
        if (!item.mesh) continue;

        // Transform
        TransformCB transformData;
        auto mvp = item.worldMatrix * viewMatrix * projection;
        DirectX::XMStoreFloat4x4(&transformData.world, DirectX::XMMatrixTranspose(item.worldMatrix.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.view, DirectX::XMMatrixTranspose(viewMatrix.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.projection, DirectX::XMMatrixTranspose(projection.GetXMMatrix()));
        DirectX::XMStoreFloat4x4(&transformData.mvp, DirectX::XMMatrixTranspose(mvp.GetXMMatrix()));
        constantBuffer_.Update(transformData);
        cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress());

        // Texture
        if (item.material) {
            cmdList->SetGraphicsRootDescriptorTable(1, item.material->GetAlbedoSRV(heap));
        }

        // Material
        MaterialCB materialData;
        if (item.material) {
            const auto& matData = item.material->GetData();
            materialData.albedo = DirectX::XMFLOAT3(matData.albedo[0], matData.albedo[1], matData.albedo[2]);
            materialData.metallic = matData.metallic;
            materialData.roughness = matData.roughness;
        } else {
            materialData.albedo = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
            materialData.metallic = 0.0f;
            materialData.roughness = 0.5f;
        }
        materialBuffer_.Update(materialData);
        cmdList->SetGraphicsRootConstantBufferView(3, materialBuffer_.GetGPUAddress());

        // Bone matrices
        BoneMatricesCB boneData = {};
        if (item.boneMatrices) {
            size_t numBones = (std::min)(item.boneMatrices->size(), static_cast<size_t>(MAX_BONES));
            for (size_t i = 0; i < numBones; ++i) {
                DirectX::XMStoreFloat4x4(&boneData.bones[i],
                    DirectX::XMMatrixTranspose((*item.boneMatrices)[i].GetXMMatrix()));
            }
        } else {
            for (int i = 0; i < MAX_BONES; ++i) {
                DirectX::XMStoreFloat4x4(&boneData.bones[i], DirectX::XMMatrixIdentity());
            }
        }
        boneBuffer_.Update(boneData);
        cmdList->SetGraphicsRootConstantBufferView(4, boneBuffer_.GetGPUAddress());

        // Draw
        auto vbView = item.mesh->GetVertexBuffer().GetView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);
        auto ibView = item.mesh->GetIndexBuffer().GetView();
        cmdList->IASetIndexBuffer(&ibView);

        uint32 indexCount = item.mesh->GetIndexBuffer().GetIndexCount();
        char drawDebug[256];
        sprintf_s(drawDebug, "Drawing skinned mesh: %u indices\n", indexCount);
        OutputDebugStringA(drawDebug);

        cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    }
}

} // namespace UnoEngine
