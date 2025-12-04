#include "pch.h"
#include "Renderer.h"
#include "../Core/Scene.h"
#include "../Core/Logger.h"
#include "../Graphics/DirectionalLightComponent.h"
#include "../Graphics/Shader.h"
#include "../Animation/Animator.h"
#include <imgui.h>
#include <Windows.h>

namespace UnoEngine {

// Matrix4x4をFloat4x4に変換（転置して格納）
static void StoreTransposedMatrix(Float4x4& dest, const Matrix4x4& src) {
    Matrix4x4 transposed = src.Transpose();
    transposed.ToFloatArray(reinterpret_cast<float*>(&dest));
}

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

    constantBuffer_.Create(device, 512);  // フレーム内で複数ビュー×複数メッシュ分
    lightBuffer_.Create(device, 16);       // 複数ビュー分
    materialBuffer_.Create(device, 512);   // フレーム内で複数ビュー×複数メッシュ分
    boneBuffer_.Create(device);
    
    // スキンメッシュ用ダイナミックバッファ（フレーム内で複数回更新可能）
    skinnedTransformBuffer_.Create(device, 256);  // 最大256個のスキンメッシュ/フレーム
    skinnedMaterialBuffer_.Create(device, 256);
    
    // StructuredBuffer for bone matrices (BoneMatrixPair)
    CreateBoneMatrixPairBuffer(device);

    imguiManager_ = MakeUnique<ImGuiManager>();
    imguiManager_->Initialize(graphics_, window_, 2);

    // デバッグレンダラー初期化
    debugRenderer_ = MakeUnique<DebugRenderer>();
    debugRenderer_->Initialize(graphics_);
}

void Renderer::BeginFrame() {
    // フレーム開始時にダイナミックバッファをリセット
    constantBuffer_.Reset();
    lightBuffer_.Reset();
    materialBuffer_.Reset();
    skinnedTransformBuffer_.Reset();
    skinnedMaterialBuffer_.Reset();
    currentBoneSlot_ = 0;
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
    lightData.directionalLightDirection = Float3(gpuLight.direction.GetX(), gpuLight.direction.GetY(), gpuLight.direction.GetZ());
    lightData.directionalLightColor = Float3(gpuLight.color.GetX(), gpuLight.color.GetY(), gpuLight.color.GetZ());
    lightData.directionalLightIntensity = gpuLight.intensity;
    lightData.ambientLight = Float3(gpuLight.ambient.GetX(), gpuLight.ambient.GetY(), gpuLight.ambient.GetZ());

    auto cameraPos = view.camera->GetPosition();
    lightData.cameraPosition = Float3(cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());

    currentLightGpuAddr_ = lightBuffer_.Update(lightData);
}

void Renderer::RenderMeshes(const RenderView& view, const std::vector<RenderItem>& items) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();

    cmdList->SetPipelineState(pipeline_.GetPipelineState());
    cmdList->SetGraphicsRootSignature(pipeline_.GetRootSignature());

    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ライトバッファはUpdateLightingで更新済み
    cmdList->SetGraphicsRootConstantBufferView(2, currentLightGpuAddr_);

    auto viewMatrix = view.camera->GetViewMatrix();
    auto projection = view.camera->GetProjectionMatrix();

    for (const auto& item : items) {
        if (!item.mesh || !item.material) continue;

        TransformCB transformData;
        auto mvp = item.worldMatrix * viewMatrix * projection;
        StoreTransposedMatrix(transformData.world, item.worldMatrix);
        StoreTransposedMatrix(transformData.view, viewMatrix);
        StoreTransposedMatrix(transformData.projection, projection);
        StoreTransposedMatrix(transformData.mvp, mvp);
        D3D12_GPU_VIRTUAL_ADDRESS transformGpuAddr = constantBuffer_.Update(transformData);
        cmdList->SetGraphicsRootConstantBufferView(0, transformGpuAddr);

        cmdList->SetGraphicsRootDescriptorTable(1, item.material->GetAlbedoSRV(heap));

        MaterialCB materialData;
        const auto& matData = item.material->GetData();
        materialData.albedo = Float3(matData.albedo[0], matData.albedo[1], matData.albedo[2]);
        materialData.metallic = matData.metallic;
        materialData.roughness = matData.roughness;
        D3D12_GPU_VIRTUAL_ADDRESS materialGpuAddr = materialBuffer_.Update(materialData);
        cmdList->SetGraphicsRootConstantBufferView(3, materialGpuAddr);

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
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(window_->GetWidth());
    viewport.Height = static_cast<float>(window_->GetHeight());
    viewport.MinDepth = 0.0f;
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
                             const std::vector<SkinnedRenderItem>& skinnedItems,
                             bool enableDebugDraw) {
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
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(desc.Width);
    viewport.Height = static_cast<float>(desc.Height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.right = static_cast<LONG>(desc.Width);
    scissorRect.bottom = static_cast<LONG>(desc.Height);

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    // グリッド描画（最初に描画）
    if (enableDebugDraw && debugRenderer_) {
        debugRenderer_->RenderGrid(
            cmdList,
            view.camera->GetViewMatrix(),
            view.camera->GetProjectionMatrix(),
            view.camera->GetPosition()
        );
    }

    // Render scene
    UpdateLighting(view, lightManager);
    RenderMeshes(view, items);

    // Render skinned meshes
    if (!skinnedItems.empty()) {
        RenderSkinnedMeshes(view, skinnedItems);
    }

    // デバッグ描画（enableDebugDrawがtrueの場合のみ）
    // 注意: BeginFrame()は呼び出し側（GameApplication等）で管理する
    // ここではボーン描画とライン描画のみ行う
    if (enableDebugDraw && debugRenderer_) {
        // ボーン描画が有効な場合
        if (debugRenderer_->GetShowBones()) {
            // テスト用: 原点に軸を描画（パイプライン動作確認）
            debugRenderer_->AddLine(Vector3(0, 0, 0), Vector3(0, 0.3f, 0), Vector4(1, 0, 0, 1));  // 赤Y軸
            debugRenderer_->AddLine(Vector3(0, 0, 0), Vector3(0.3f, 0, 0), Vector4(0, 1, 0, 1));  // 緑X軸
            debugRenderer_->AddLine(Vector3(0, 0, 0), Vector3(0, 0, 0.3f), Vector4(0, 0, 1, 1));  // 青Z軸

            for (const auto& item : skinnedItems) {
                if (item.animator) {
                    auto* skeleton = item.animator->GetSkeleton();
                    const auto& localTransforms = item.animator->GetCurrentLocalTransforms();
                    if (skeleton && !localTransforms.empty()) {
                        debugRenderer_->DrawBones(skeleton, localTransforms, item.worldMatrix);
                    }
                }
            }
        }

        // デバッグライン描画（カメラギズモ、ボーン等すべて）
        debugRenderer_->Render(
            cmdList,
            view.camera->GetViewMatrix(),
            view.camera->GetProjectionMatrix()
        );
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

#ifdef _DEBUG
    // デバッグボーン描画
    // 注意: BeginFrame()は呼び出し側で管理する
    if (debugRenderer_ && debugRenderer_->GetShowBones()) {
        for (const auto& item : items) {
            if (item.animator) {
                auto* skeleton = item.animator->GetSkeleton();
                const auto& localTransforms = item.animator->GetCurrentLocalTransforms();
                if (skeleton && !localTransforms.empty()) {
                    debugRenderer_->DrawBones(skeleton, localTransforms, item.worldMatrix);
                }
            }
        }

        debugRenderer_->Render(
            graphics_->GetCommandList(),
            view.camera->GetViewMatrix(),
            view.camera->GetProjectionMatrix()
        );
    }
#endif
}

void Renderer::RenderSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();

    cmdList->SetPipelineState(skinnedPipeline_.GetPipelineState());
    cmdList->SetGraphicsRootSignature(skinnedPipeline_.GetRootSignature());

    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ライトバッファはUpdateLightingで更新済み
    cmdList->SetGraphicsRootConstantBufferView(2, currentLightGpuAddr_);

    auto viewMatrix = view.camera->GetViewMatrix();
    auto projection = view.camera->GetProjectionMatrix();

    // 注意: ダイナミックバッファのリセットはRenderer::BeginFrame()で行われる

    // ボーン行列バッファ全体を一度だけマップ
    BoneMatrixPair* mappedBoneData = nullptr;
    if (boneMatrixPairBuffer_) {
        boneMatrixPairBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBoneData));
    }

    for (const auto& item : items) {
        if (!item.mesh) continue;

        // boneMatrixPairsがない場合は描画をスキップ
        if (!item.boneMatrixPairs || item.boneMatrixPairs->empty()) {
            continue;
        }

        // スロット数を超えた場合はスキップ
        if (currentBoneSlot_ >= MAX_SKINNED_OBJECTS) {
            Logger::Warning("[Renderer] Max skinned objects ({}) exceeded, skipping", MAX_SKINNED_OBJECTS);
            break;
        }

        // Transform（ダイナミックバッファを使用）
        TransformCB transformData;
        auto mvp = item.worldMatrix * viewMatrix * projection;
        StoreTransposedMatrix(transformData.world, item.worldMatrix);
        StoreTransposedMatrix(transformData.view, viewMatrix);
        StoreTransposedMatrix(transformData.projection, projection);
        StoreTransposedMatrix(transformData.mvp, mvp);
        auto transformGpuAddr = skinnedTransformBuffer_.Update(transformData);
        cmdList->SetGraphicsRootConstantBufferView(0, transformGpuAddr);

        // Texture
        if (item.material) {
            cmdList->SetGraphicsRootDescriptorTable(4, item.material->GetAlbedoSRV(heap));
        }

        // Material（ダイナミックバッファを使用）
        MaterialCB materialData;
        if (item.material) {
            const auto& matData = item.material->GetData();
            materialData.albedo = Float3(matData.albedo[0], matData.albedo[1], matData.albedo[2]);
            materialData.metallic = matData.metallic;
            materialData.roughness = matData.roughness;
        } else {
            materialData.albedo = Float3(1.0f, 1.0f, 1.0f);
            materialData.metallic = 0.0f;
            materialData.roughness = 0.5f;
        }
        auto materialGpuAddr = skinnedMaterialBuffer_.Update(materialData);
        cmdList->SetGraphicsRootConstantBufferView(3, materialGpuAddr);

        // Bone matrices（現在のスロットに書き込み）
        if (mappedBoneData && item.boneMatrixPairs) {
            size_t numBones = (std::min)(item.boneMatrixPairs->size(), static_cast<size_t>(MAX_BONES));
            
            // このスロットのオフセット
            BoneMatrixPair* slotData = mappedBoneData + (currentBoneSlot_ * MAX_BONES);
            
            for (size_t i = 0; i < numBones; ++i) {
                const auto& pair = (*item.boneMatrixPairs)[i];
                // 転置した行列を格納
                Matrix4x4 transposedSkeleton = pair.skeletonSpaceMatrix.Transpose();
                Matrix4x4 transposedInvTranspose = pair.skeletonSpaceInverseTransposeMatrix.Transpose();
                transposedSkeleton.ToFloatArray(reinterpret_cast<float*>(&slotData[i].skeletonSpaceMatrix));
                transposedInvTranspose.ToFloatArray(reinterpret_cast<float*>(&slotData[i].skeletonSpaceInverseTransposeMatrix));
            }
            
            // このスロット用のSRVをバインド
            cmdList->SetGraphicsRootDescriptorTable(1, boneMatrixPairSRVs_[currentBoneSlot_]);
            currentBoneSlot_++;
        }

        // Draw
        auto vbView = item.mesh->GetVertexBuffer().GetView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);
        auto ibView = item.mesh->GetIndexBuffer().GetView();
        cmdList->IASetIndexBuffer(&ibView);

        uint32 indexCount = item.mesh->GetIndexBuffer().GetIndexCount();
        cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    }

    // ボーン行列バッファのアンマップ
    if (boneMatrixPairBuffer_) {
        boneMatrixPairBuffer_->Unmap(0, nullptr);
    }
}

void Renderer::CreateBoneMatrixPairBuffer(ID3D12Device* device) {
    // StructuredBuffer for bone matrices（複数モデル対応）
    // MAX_SKINNED_OBJECTS個のスロットを持つ大きなバッファを作成
    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    // 各スロットに MAX_BONES 個のボーン行列を格納
    const size_t slotSize = sizeof(BoneMatrixPair) * MAX_BONES;
    const size_t totalSize = slotSize * MAX_SKINNED_OBJECTS;
    
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = totalSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    
    HRESULT hr = device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&boneMatrixPairBuffer_)
    );
    
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create bone matrix pair buffer\n");
        return;
    }
    
    // 各スロット用のSRVを作成（インデックス2048から開始）
    boneMatrixPairSRVBaseIndex_ = 2048;
    
    auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    for (uint32 slot = 0; slot < MAX_SKINNED_OBJECTS; ++slot) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = slot * MAX_BONES;  // 各スロットのオフセット
        srvDesc.Buffer.NumElements = MAX_BONES;
        srvDesc.Buffer.StructureByteStride = sizeof(BoneMatrixPair);
        
        auto cpuHandle = graphics_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart();
        cpuHandle.ptr += (boneMatrixPairSRVBaseIndex_ + slot) * descriptorSize;
        
        device->CreateShaderResourceView(boneMatrixPairBuffer_.Get(), &srvDesc, cpuHandle);
        
        boneMatrixPairSRVs_[slot] = graphics_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
        boneMatrixPairSRVs_[slot].ptr += (boneMatrixPairSRVBaseIndex_ + slot) * descriptorSize;
    }
}

} // namespace UnoEngine
