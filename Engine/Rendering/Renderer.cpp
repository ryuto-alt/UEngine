#include "pch.h"
#include "Renderer.h"
#include "../Core/Scene.h"
#include "../Core/Logger.h"
#include "../Graphics/DirectionalLightComponent.h"
#include "../Graphics/ShadowMap.h"
#include "../Graphics/Shader.h"
#include "../Animation/Animator.h"
#include "../Video/VideoPlayerComponent.h"
#include <imgui.h>
#include <Windows.h>

namespace UnoEngine {

namespace {

constexpr uint32_t kConstantBufferCount  = 512;
constexpr uint32_t kLightBufferCount     = 16;
constexpr uint32_t kMaterialBufferCount  = 512;
constexpr uint32_t kSkinnedBufferCount   = 256;
constexpr uint32_t kShadowBufferCount    = 768; // static + skinned shadow pass slots

void StoreTransposedMatrix(Float4x4& dest, const Matrix4x4& src) {
    Matrix4x4 transposed = src.Transpose();
    transposed.ToFloatArray(reinterpret_cast<float*>(&dest));
}

} // anonymous namespace

void Renderer::Initialize(GraphicsDevice* graphics, Window* window) {
    graphics_ = graphics;
    window_ = window;

    auto* device = graphics_->GetDevice();

    // PBR Pipeline
    Shader vertexShader;
    vertexShader.CompileFromFile(L"Shaders/PBR/PBRVS.hlsl", ShaderStage::Vertex);

    Shader pixelShader;
    pixelShader.CompileFromFile(L"Shaders/PBR/PBRPS.hlsl", ShaderStage::Pixel);

    pipeline_.Initialize(device, vertexShader, pixelShader, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    // Skinned Pipeline
    Shader skinnedVS;
    skinnedVS.CompileFromFile(L"Shaders/Skinned/SkinnedVS.hlsl", ShaderStage::Vertex);

    Shader skinnedPS;
    skinnedPS.CompileFromFile(L"Shaders/Skinned/SkinnedPS.hlsl", ShaderStage::Pixel);

    skinnedPipeline_.Initialize(device, skinnedVS, skinnedPS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    constantBuffer_.Create(device, kConstantBufferCount);
    lightBuffer_.Create(device, kLightBufferCount);
    materialBuffer_.Create(device, kMaterialBufferCount);
    boneBuffer_.Create(device);

    skinnedTransformBuffer_.Create(device, kSkinnedBufferCount);
    skinnedMaterialBuffer_.Create(device, kSkinnedBufferCount);

    // StructuredBuffer for bone matrices (BoneMatrixPair)
    CreateBoneMatrixPairBuffer(device);

    outlinePipeline_.Initialize(graphics_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D32_FLOAT);
    outlineCB_.Create(device, 1);

    shadowMap_.Create(graphics_, 2048);
    for (int i = 0; i < MAX_SPOT_SHADOWS; ++i) {
        spotShadowMaps_[i].Create(graphics_, 1024);
    }
    shadowPipeline_.Initialize(device);
    shadowTransformBuffer_.Create(device, kShadowBufferCount);

    imguiManager_ = MakeUnique<ImGuiManager>();
    imguiManager_->Initialize(graphics_, window_, 2);

    // デバッグレンダラー初期化
    debugRenderer_ = MakeUnique<DebugRenderer>();
    debugRenderer_->Initialize(graphics_);

    // 草原システム初期化
    grassRenderer_.Initialize(graphics_);

    // 草メッシュとデフォルトテクスチャのGPUアップロード
    graphics_->BeginResourceUpload();
    grassSystem_.Initialize(graphics_);
    if (!grassRenderer_.LoadTextureFromFile("assets/tex/grass/Foliage/Foliage006.png")) {
        grassRenderer_.CreateFallbackTexture(graphics_, graphics_->GetCommandList());
    }
    graphics_->EndResourceUpload();
}

void Renderer::BeginFrame() {
    constantBuffer_.Reset();
    lightBuffer_.Reset();
    materialBuffer_.Reset();
    skinnedTransformBuffer_.Reset();
    skinnedMaterialBuffer_.Reset();
    outlineCB_.Reset();
    shadowTransformBuffer_.Reset();
    currentBoneSlot_ = 0;

    // 草原システム更新（風アニメ用タイマー）
    grassRenderer_.Update(1.0f / 60.0f); // TODO: 実際のデルタタイムを渡す
}

void Renderer::Draw(const RenderView& view, const std::vector<RenderItem>& items, LightManager* lights, Scene* scene) {
    if (!view.camera) return;

    auto* cmdList = graphics_->GetCommandList();

    if (scene) {
        for (const auto& obj : scene->GetGameObjects()) {
            auto* videoPlayer = obj->GetComponent<VideoPlayerComponent>();
            if (videoPlayer && videoPlayer->HasPendingFrame()) {
                videoPlayer->UploadVideoFrame(cmdList);
            }
        }
    }

    Matrix4x4 lightViewProj;
    UpdateLighting(view, lights, lightViewProj);
    RenderShadowMap(items, {}, lightViewProj);
    RenderSpotShadowMaps(items, {});
    currentBoneSlot_ = 0;

    // シャドウマップ描画でRTVが変わるため、バックバッファに戻す
    graphics_->SetBackBufferAsRenderTarget();
    SetupViewport();
    RenderMeshes(view, items, lightViewProj);

    // 草原描画
    if (grassSystem_.GetInstanceCount() > 0) {
        grassRenderer_.Render(view, lightViewProj, spotLightViewProjs_,
                              activeSpotShadowCount_, currentLightGpuAddr_,
                              shadowMap_, spotShadowMaps_, grassSystem_);
    }

    RenderUI(scene);

    shadowMap_.RestoreForNextFrame(cmdList);
    for (int i = 0; i < activeSpotShadowCount_; ++i) {
        spotShadowMaps_[i].RestoreForNextFrame(cmdList);
    }
}

void Renderer::UpdateLighting(const RenderView& view, LightManager* lights, Matrix4x4& outLightViewProj) {
    auto gpuLight = lights ? lights->BuildGPULightData() : GPULightData{};

    LightCB lightData{};
    lightData.directionalLightDirection = Float3(gpuLight.direction.GetX(), gpuLight.direction.GetY(), gpuLight.direction.GetZ());
    lightData.directionalLightColor     = Float3(gpuLight.color.GetX(), gpuLight.color.GetY(), gpuLight.color.GetZ());
    lightData.directionalLightIntensity = gpuLight.intensity;
    lightData.ambientLight              = Float3(gpuLight.ambient.GetX(), gpuLight.ambient.GetY(), gpuLight.ambient.GetZ());

    auto cameraPos = view.camera->GetPosition();
    lightData.cameraPosition = Float3(cameraPos.GetX(), cameraPos.GetY(), cameraPos.GetZ());

    // Point lights (max 8)
    auto pointCount = static_cast<int32_t>(std::min(gpuLight.pointLights.size(), size_t(8)));
    lightData.pointLightCount = pointCount;
    for (int32_t i = 0; i < pointCount; ++i) {
        const auto& pl = gpuLight.pointLights[i];
        lightData.pointLights[i].position  = Float3(pl.position.GetX(), pl.position.GetY(), pl.position.GetZ());
        lightData.pointLights[i].range     = pl.range;
        lightData.pointLights[i].color     = Float3(pl.color.GetX(), pl.color.GetY(), pl.color.GetZ());
        lightData.pointLights[i].intensity = pl.intensity;
    }

    // Spot lights (max 4)
    auto spotCount = static_cast<int32_t>(std::min(gpuLight.spotLights.size(), size_t(4)));
    lightData.spotLightCount = spotCount;
    for (int32_t i = 0; i < spotCount; ++i) {
        const auto& sl = gpuLight.spotLights[i];
        lightData.spotLights[i].position   = Float3(sl.position.GetX(), sl.position.GetY(), sl.position.GetZ());
        lightData.spotLights[i].range      = sl.range;
        lightData.spotLights[i].direction  = Float3(sl.direction.GetX(), sl.direction.GetY(), sl.direction.GetZ());
        lightData.spotLights[i].spotAngle  = sl.spotAngle;
        lightData.spotLights[i].color      = Float3(sl.color.GetX(), sl.color.GetY(), sl.color.GetZ());
        lightData.spotLights[i].intensity  = sl.intensity;
        lightData.spotLights[i].innerAngle = sl.innerAngle;
    }

    lightData.shadowBias = 0.001f;

    // Compute spot light shadow ViewProjs
    activeSpotShadowCount_ = static_cast<int32_t>(std::min(spotCount, static_cast<int32_t>(MAX_SPOT_SHADOWS)));
    lightData.spotShadowCount = activeSpotShadowCount_;

    for (int32_t i = 0; i < activeSpotShadowCount_; ++i) {
        const auto& sl = gpuLight.spotLights[i];
        spotLightViewProjs_[i] = ShadowMap::ComputeSpotLightViewProj(
            sl.position, sl.direction, sl.spotAngle, sl.range);
    }

    currentLightGpuAddr_ = lightBuffer_.Update(lightData);

    // Compute directional light view-projection for shadow map (centered on camera)
    Vector3 shadowCenter = view.camera ? view.camera->GetPosition() : Vector3(0.0f, 0.0f, 0.0f);
    outLightViewProj = ShadowMap::ComputeLightViewProj(gpuLight.direction, shadowCenter, 80.0f);
    lastLightViewProj_ = outLightViewProj;
}

void Renderer::RenderMeshes(const RenderView& view, const std::vector<RenderItem>& items, const Matrix4x4& lightViewProj) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();

    cmdList->SetGraphicsRootSignature(pipeline_.GetRootSignature());

    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmdList->SetGraphicsRootConstantBufferView(2, currentLightGpuAddr_);
    cmdList->SetGraphicsRootDescriptorTable(4, shadowMap_.GetSRVHandle()); // shadow map at [4]

    // Bind spot shadow maps at [5] - always bind (contiguous SRVs from initialization)
    cmdList->SetGraphicsRootDescriptorTable(5, spotShadowMaps_[0].GetSRVHandle());

    auto viewMatrix = view.camera->GetViewMatrix();
    auto projection = view.camera->GetProjectionMatrix();

    // 3-pass rendering: opaque → alpha-test(MASK) → alpha-blend(BLEND)
    for (int pass = 0; pass < 3; ++pass) {
        if (pass == 0)
            cmdList->SetPipelineState(pipeline_.GetPipelineState());
        else if (pass == 1)
            cmdList->SetPipelineState(pipeline_.GetAlphaTestPipelineState());
        else
            cmdList->SetPipelineState(pipeline_.GetAlphaBlendPipelineState());

        for (const auto& item : items) {
            if (!item.mesh || !item.material) continue;

            const auto& matData = item.material->GetData();

            // パス振り分け: pass0=不透明, pass1=アルファテスト(MASK), pass2=アルファブレンド(BLEND)
            if (pass == 0) {
                if (matData.useAlphaClip || matData.useAlphaBlend || matData.doubleSided) continue;
            } else if (pass == 1) {
                if (matData.useAlphaBlend) continue;
                if (!(matData.useAlphaClip || matData.doubleSided)) continue;
            } else {
                if (!matData.useAlphaBlend) continue;
            }

            TransformCB transformData{};
            auto mvp = item.worldMatrix * viewMatrix * projection;
            StoreTransposedMatrix(transformData.world, item.worldMatrix);
            StoreTransposedMatrix(transformData.view, viewMatrix);
            StoreTransposedMatrix(transformData.projection, projection);
            StoreTransposedMatrix(transformData.mvp, mvp);
            StoreTransposedMatrix(transformData.lightViewProj, lightViewProj);
            for (int si = 0; si < activeSpotShadowCount_; ++si) {
                StoreTransposedMatrix(transformData.spotLightViewProj[si], spotLightViewProjs_[si]);
            }
            D3D12_GPU_VIRTUAL_ADDRESS transformGpuAddr = constantBuffer_.Update(transformData);
            cmdList->SetGraphicsRootConstantBufferView(0, transformGpuAddr);

            cmdList->SetGraphicsRootDescriptorTable(1, item.material->GetAlbedoSRV(heap));

            MaterialCB materialData{};
            materialData.albedo    = Float3(matData.albedo[0], matData.albedo[1], matData.albedo[2]);
            materialData.metallic  = matData.metallic;
            materialData.roughness = matData.roughness;
            materialData.alphaClipThreshold = matData.useAlphaClip ? matData.alphaClipThreshold : 0.0f;
            materialData.doubleSided = matData.doubleSided ? 1.0f : 0.0f;
            materialData.useAlphaBlend = matData.useAlphaBlend ? 1.0f : 0.0f;
            D3D12_GPU_VIRTUAL_ADDRESS materialGpuAddr = materialBuffer_.Update(materialData);
            cmdList->SetGraphicsRootConstantBufferView(3, materialGpuAddr);

            auto vbView = item.mesh->GetVertexBuffer().GetView();
            cmdList->IASetVertexBuffers(0, 1, &vbView);
            auto ibView = item.mesh->GetIndexBuffer().GetView();
            cmdList->IASetIndexBuffer(&ibView);
            cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
        }
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

#ifdef WITH_EDITOR
void Renderer::RenderLoadingScreen(std::string_view message, float progress) {
    SetupViewport();
    auto* imgui   = imguiManager_.get();
    auto* cmdList = graphics_->GetCommandList();

    // ImGui の DX12 バックエンドはフォントテクスチャ参照時に SRV ヒープが必要
    ID3D12DescriptorHeap* heaps[] = { graphics_->GetSRVHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    imgui->BeginFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 sz   = io.DisplaySize;

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(sz);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("##LoadingScreen", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs     | ImGuiWindowFlags_NoNav  |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // ウィンドウのDrawListを使用（BackgroundDrawListだとウィンドウの後ろに隠れる）
    ImDrawList* dl = ImGui::GetWindowDrawList();

    constexpr float barW = 420.0f;
    constexpr float barH = 12.0f;
    float cx = sz.x * 0.5f;
    float cy = sz.y * 0.5f;

    // スピナー（くるくる）
    {
        using namespace std::chrono;
        static auto startTime = high_resolution_clock::now();
        float elapsed = duration<float>(high_resolution_clock::now() - startTime).count();

        constexpr float radius    = 18.0f;
        constexpr float thickness = 3.0f;
        constexpr int   segments  = 30;
        constexpr float arcLen    = 3.14159265f * 1.4f; // 弧の長さ（約252°）
        float spinAngle = elapsed * 4.0f; // 回転速度

        ImVec2 spinCenter = { cx, cy - 56.0f };

        for (int i = 0; i < segments; ++i) {
            float t0 = static_cast<float>(i) / static_cast<float>(segments);
            float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
            float a0 = spinAngle + t0 * arcLen;
            float a1 = spinAngle + t1 * arcLen;

            // フェードイン: 先頭が明るく、末尾が薄い
            uint8_t alpha = static_cast<uint8_t>(40 + 215 * t0);
            ImU32 col = IM_COL32(100, 160, 255, alpha);

            ImVec2 p0 = { spinCenter.x + cosf(a0) * radius, spinCenter.y + sinf(a0) * radius };
            ImVec2 p1 = { spinCenter.x + cosf(a1) * radius, spinCenter.y + sinf(a1) * radius };
            dl->AddLine(p0, p1, col, thickness);
        }
    }

    // メッセージテキスト
    ImVec2 textSize = ImGui::CalcTextSize(message.data());
    ImGui::SetCursorPos({cx - textSize.x * 0.5f, cy - 28.0f});
    ImGui::TextUnformatted(message.data());

    // プログレスバー
    ImGui::SetCursorPos({cx - barW * 0.5f, cy + 0.0f});
    ImGui::ProgressBar(progress, {barW, barH}, "");

    // パーセント表示
    {
        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(progress * 100.0f));
        ImVec2 pctSize = ImGui::CalcTextSize(pctBuf);
        ImGui::SetCursorPos({cx - pctSize.x * 0.5f, cy + 18.0f});
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "%s", pctBuf);
    }

    ImGui::End();

    imgui->EndFrame();
    imgui->Render(cmdList);
}
#endif

void Renderer::DrawToTexture(ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                             D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, const RenderView& view,
                             const std::vector<RenderItem>& items, LightManager* lightManager,
                             const std::vector<SkinnedRenderItem>& skinnedItems,
                             bool enableDebugDraw,
                             std::span<const RenderItem> outlineItems,
                             std::span<const SkinnedRenderItem> outlineSkinnedItems) {
    if (!view.camera) return;

    auto* cmdList = graphics_->GetCommandList();

    // Shadow pass (before scene RT barrier)
    Matrix4x4 lightViewProj;
    UpdateLighting(view, lightManager, lightViewProj);
    RenderShadowMap(items, skinnedItems, lightViewProj);
    RenderSpotShadowMaps(items, skinnedItems);
    currentBoneSlot_ = 0;

    // Resource barrier: PIXEL_SHADER_RESOURCE -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTarget;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = {0.2f, 0.3f, 0.4f, 1.0f};
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_RESOURCE_DESC desc = renderTarget->GetDesc();
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = static_cast<float>(desc.Width);
    viewport.Height   = static_cast<float>(desc.Height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.right  = static_cast<LONG>(desc.Width);
    scissorRect.bottom = static_cast<LONG>(desc.Height);

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    if (enableDebugDraw && debugRenderer_) {
        debugRenderer_->RenderGrid(
            cmdList,
            view.camera->GetViewMatrix(),
            view.camera->GetProjectionMatrix(),
            view.camera->GetPosition()
        );
    }

    // Main scene pass (shadow map is now in PSR state)
    RenderMeshes(view, items, lightViewProj);

    if (!skinnedItems.empty()) {
        RenderSkinnedMeshes(view, skinnedItems, lightViewProj);
    }

    // 草原描画
    if (grassSystem_.GetInstanceCount() > 0) {
        grassRenderer_.Render(view, lightViewProj, spotLightViewProjs_,
                              activeSpotShadowCount_, currentLightGpuAddr_,
                              shadowMap_, spotShadowMaps_, grassSystem_);
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

    // Outline pass (after debug draw, before final barrier)
    if (!outlineItems.empty() || !outlineSkinnedItems.empty()) {
        RenderOutline(view, outlineItems, outlineSkinnedItems);
    }

    // Resource barrier: RENDER_TARGET -> PIXEL_SHADER_RESOURCE
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    // Restore shadow map to DEPTH_WRITE for next frame
    shadowMap_.RestoreForNextFrame(cmdList);
    for (int i = 0; i < activeSpotShadowCount_; ++i) {
        spotShadowMaps_[i].RestoreForNextFrame(cmdList);
    }
}

void Renderer::DrawSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items, LightManager* lights) {
    if (!view.camera) return;

    SetupViewport();
    Matrix4x4 lightViewProj;
    UpdateLighting(view, lights, lightViewProj);
    RenderSkinnedMeshes(view, items, lightViewProj);

#ifdef WITH_EDITOR
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

void Renderer::RenderSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items, const Matrix4x4& lightViewProj) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();

    cmdList->SetPipelineState(skinnedPipeline_.GetPipelineState());
    cmdList->SetGraphicsRootSignature(skinnedPipeline_.GetRootSignature());

    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmdList->SetGraphicsRootConstantBufferView(2, currentLightGpuAddr_);
    cmdList->SetGraphicsRootDescriptorTable(5, shadowMap_.GetSRVHandle()); // shadow at [5]

    // Bind spot shadow maps at [6] - always bind (contiguous SRVs from initialization)
    cmdList->SetGraphicsRootDescriptorTable(6, spotShadowMaps_[0].GetSRVHandle());

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

        TransformCB transformData{};
        auto mvp = item.worldMatrix * viewMatrix * projection;
        StoreTransposedMatrix(transformData.world, item.worldMatrix);
        StoreTransposedMatrix(transformData.view, viewMatrix);
        StoreTransposedMatrix(transformData.projection, projection);
        StoreTransposedMatrix(transformData.mvp, mvp);
        StoreTransposedMatrix(transformData.lightViewProj, lightViewProj);
        for (int si = 0; si < activeSpotShadowCount_; ++si) {
            StoreTransposedMatrix(transformData.spotLightViewProj[si], spotLightViewProjs_[si]);
        }
        auto transformGpuAddr = skinnedTransformBuffer_.Update(transformData);
        cmdList->SetGraphicsRootConstantBufferView(0, transformGpuAddr);

        // Texture
        if (item.material) {
            cmdList->SetGraphicsRootDescriptorTable(4, item.material->GetAlbedoSRV(heap));
        }

        // Material（ダイナミックバッファを使用）
        MaterialCB materialData{};
        if (item.material) {
            const auto& matData = item.material->GetData();
            materialData.albedo = Float3(matData.albedo[0], matData.albedo[1], matData.albedo[2]);
            materialData.metallic = matData.metallic;
            materialData.roughness = matData.roughness;
            materialData.alphaClipThreshold = 0.0f;
        } else {
            materialData.albedo = Float3(1.0f, 1.0f, 1.0f);
            materialData.metallic = 0.0f;
            materialData.roughness = 0.5f;
            materialData.alphaClipThreshold = 0.0f;
        }
        auto materialGpuAddr = skinnedMaterialBuffer_.Update(materialData);
        cmdList->SetGraphicsRootConstantBufferView(3, materialGpuAddr);

        // Bone matrices（現在のスロットに書き込み）
        if (mappedBoneData && item.boneMatrixPairs) {
            size_t numBones = (std::min)(item.boneMatrixPairs->size(), static_cast<size_t>(MAX_BONES));

            BoneMatrixPair* slotData = mappedBoneData + (currentBoneSlot_ * MAX_BONES);

            for (size_t i = 0; i < numBones; ++i) {
                const auto& pair = (*item.boneMatrixPairs)[i];
                Matrix4x4 transposedSkeleton = pair.skeletonSpaceMatrix.Transpose();
                Matrix4x4 transposedInvTranspose = pair.skeletonSpaceInverseTransposeMatrix.Transpose();
                transposedSkeleton.ToFloatArray(reinterpret_cast<float*>(&slotData[i].skeletonSpaceMatrix));
                transposedInvTranspose.ToFloatArray(reinterpret_cast<float*>(&slotData[i].skeletonSpaceInverseTransposeMatrix));
            }

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

void Renderer::RenderShadowMap(const std::vector<RenderItem>& items,
                               const std::vector<SkinnedRenderItem>& skinnedItems,
                               const Matrix4x4& lightViewProj) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap    = graphics_->GetSRVHeap();

    shadowMap_.BeginShadowPass(cmdList);

    // Static meshes (depth-only)
    if (!items.empty()) {
        cmdList->SetPipelineState(shadowPipeline_.GetStaticPSO());
        cmdList->SetGraphicsRootSignature(shadowPipeline_.GetStaticRootSignature());
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (const auto& item : items) {
            if (!item.mesh) continue;

            ShadowTransformCB shadowData{};
            StoreTransposedMatrix(shadowData.world, item.worldMatrix);
            StoreTransposedMatrix(shadowData.lightViewProj, lightViewProj);
            auto gpuAddr = shadowTransformBuffer_.Update(shadowData);
            cmdList->SetGraphicsRootConstantBufferView(0, gpuAddr);

            auto vbView = item.mesh->GetVertexBuffer().GetView();
            cmdList->IASetVertexBuffers(0, 1, &vbView);
            auto ibView = item.mesh->GetIndexBuffer().GetView();
            cmdList->IASetIndexBuffer(&ibView);
            cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
        }
    }

    // Skinned meshes (depth-only, bones from existing buffer)
    if (!skinnedItems.empty() && boneMatrixPairBuffer_) {
        ID3D12DescriptorHeap* heaps[] = { heap };
        cmdList->SetDescriptorHeaps(1, heaps);

        cmdList->SetPipelineState(shadowPipeline_.GetSkinnedPSO());
        cmdList->SetGraphicsRootSignature(shadowPipeline_.GetSkinnedRootSignature());
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        BoneMatrixPair* mappedBoneData = nullptr;
        boneMatrixPairBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBoneData));

        for (const auto& item : skinnedItems) {
            if (!item.mesh || !item.boneMatrixPairs || item.boneMatrixPairs->empty()) continue;
            if (currentBoneSlot_ >= MAX_SKINNED_OBJECTS) break;

            ShadowTransformCB shadowData{};
            StoreTransposedMatrix(shadowData.world, item.worldMatrix);
            StoreTransposedMatrix(shadowData.lightViewProj, lightViewProj);
            auto gpuAddr = shadowTransformBuffer_.Update(shadowData);
            cmdList->SetGraphicsRootConstantBufferView(0, gpuAddr);

            size_t numBones = std::min(item.boneMatrixPairs->size(), static_cast<size_t>(MAX_BONES));
            BoneMatrixPair* slotData = mappedBoneData + (currentBoneSlot_ * MAX_BONES);
            for (size_t i = 0; i < numBones; ++i) {
                const auto& pair = (*item.boneMatrixPairs)[i];
                Matrix4x4 ts = pair.skeletonSpaceMatrix.Transpose();
                Matrix4x4 ti = pair.skeletonSpaceInverseTransposeMatrix.Transpose();
                ts.ToFloatArray(reinterpret_cast<float*>(&slotData[i].skeletonSpaceMatrix));
                ti.ToFloatArray(reinterpret_cast<float*>(&slotData[i].skeletonSpaceInverseTransposeMatrix));
            }
            cmdList->SetGraphicsRootDescriptorTable(1, boneMatrixPairSRVs_[currentBoneSlot_]);
            currentBoneSlot_++;

            auto vbView = item.mesh->GetVertexBuffer().GetView();
            cmdList->IASetVertexBuffers(0, 1, &vbView);
            auto ibView = item.mesh->GetIndexBuffer().GetView();
            cmdList->IASetIndexBuffer(&ibView);
            cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
        }

        boneMatrixPairBuffer_->Unmap(0, nullptr);
    }

    shadowMap_.EndShadowPass(cmdList);
}

void Renderer::RenderSpotShadowMaps(const std::vector<RenderItem>& items,
                                     const std::vector<SkinnedRenderItem>& skinnedItems) {
    if (activeSpotShadowCount_ <= 0) return;

    auto* cmdList = graphics_->GetCommandList();
    auto* heap    = graphics_->GetSRVHeap();

    for (int si = 0; si < activeSpotShadowCount_; ++si) {
        spotShadowMaps_[si].BeginShadowPass(cmdList);

        // Static meshes
        if (!items.empty()) {
            cmdList->SetPipelineState(shadowPipeline_.GetStaticPSO());
            cmdList->SetGraphicsRootSignature(shadowPipeline_.GetStaticRootSignature());
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            for (const auto& item : items) {
                if (!item.mesh) continue;

                ShadowTransformCB shadowData{};
                StoreTransposedMatrix(shadowData.world, item.worldMatrix);
                StoreTransposedMatrix(shadowData.lightViewProj, spotLightViewProjs_[si]);
                auto gpuAddr = shadowTransformBuffer_.Update(shadowData);
                cmdList->SetGraphicsRootConstantBufferView(0, gpuAddr);

                auto vbView = item.mesh->GetVertexBuffer().GetView();
                cmdList->IASetVertexBuffers(0, 1, &vbView);
                auto ibView = item.mesh->GetIndexBuffer().GetView();
                cmdList->IASetIndexBuffer(&ibView);
                cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
            }
        }

        // Skinned meshes
        if (!skinnedItems.empty() && boneMatrixPairBuffer_) {
            ID3D12DescriptorHeap* heaps[] = { heap };
            cmdList->SetDescriptorHeaps(1, heaps);

            cmdList->SetPipelineState(shadowPipeline_.GetSkinnedPSO());
            cmdList->SetGraphicsRootSignature(shadowPipeline_.GetSkinnedRootSignature());
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            BoneMatrixPair* mappedBoneData = nullptr;
            boneMatrixPairBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBoneData));

            uint32 boneSlot = 0;
            for (const auto& item : skinnedItems) {
                if (!item.mesh || !item.boneMatrixPairs || item.boneMatrixPairs->empty()) continue;
                if (boneSlot >= MAX_SKINNED_OBJECTS) break;

                ShadowTransformCB shadowData{};
                StoreTransposedMatrix(shadowData.world, item.worldMatrix);
                StoreTransposedMatrix(shadowData.lightViewProj, spotLightViewProjs_[si]);
                auto gpuAddr = shadowTransformBuffer_.Update(shadowData);
                cmdList->SetGraphicsRootConstantBufferView(0, gpuAddr);

                size_t numBones = std::min(item.boneMatrixPairs->size(), static_cast<size_t>(MAX_BONES));
                BoneMatrixPair* slotData = mappedBoneData + (boneSlot * MAX_BONES);
                for (size_t bi = 0; bi < numBones; ++bi) {
                    const auto& pair = (*item.boneMatrixPairs)[bi];
                    Matrix4x4 ts = pair.skeletonSpaceMatrix.Transpose();
                    Matrix4x4 ti = pair.skeletonSpaceInverseTransposeMatrix.Transpose();
                    ts.ToFloatArray(reinterpret_cast<float*>(&slotData[bi].skeletonSpaceMatrix));
                    ti.ToFloatArray(reinterpret_cast<float*>(&slotData[bi].skeletonSpaceInverseTransposeMatrix));
                }
                cmdList->SetGraphicsRootDescriptorTable(1, boneMatrixPairSRVs_[boneSlot]);
                boneSlot++;

                auto vbView = item.mesh->GetVertexBuffer().GetView();
                cmdList->IASetVertexBuffers(0, 1, &vbView);
                auto ibView = item.mesh->GetIndexBuffer().GetView();
                cmdList->IASetIndexBuffer(&ibView);
                cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
            }

            boneMatrixPairBuffer_->Unmap(0, nullptr);
        }

        spotShadowMaps_[si].EndShadowPass(cmdList);
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

void Renderer::RenderOutline(const RenderView& view,
                             std::span<const RenderItem> outlineItems,
                             std::span<const SkinnedRenderItem> outlineSkinnedItems) {
    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();

    OutlineParamsCB outlineData;
    D3D12_GPU_VIRTUAL_ADDRESS outlineGpuAddr = outlineCB_.Update(outlineData);

    auto viewMatrix = view.camera->GetViewMatrix();
    auto projection = view.camera->GetProjectionMatrix();

    ID3D12DescriptorHeap* heaps[] = { heap };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Static mesh outlines
    if (!outlineItems.empty()) {
        cmdList->SetPipelineState(outlinePipeline_.GetStaticPSO());
        cmdList->SetGraphicsRootSignature(outlinePipeline_.GetStaticRootSignature());

        for (const auto& item : outlineItems) {
            if (!item.mesh) continue;

            TransformCB transformData;
            auto mvp = item.worldMatrix * viewMatrix * projection;
            StoreTransposedMatrix(transformData.world, item.worldMatrix);
            StoreTransposedMatrix(transformData.view, viewMatrix);
            StoreTransposedMatrix(transformData.projection, projection);
            StoreTransposedMatrix(transformData.mvp, mvp);
            D3D12_GPU_VIRTUAL_ADDRESS transformGpuAddr = constantBuffer_.Update(transformData);

            cmdList->SetGraphicsRootConstantBufferView(0, transformGpuAddr);
            cmdList->SetGraphicsRootConstantBufferView(1, outlineGpuAddr);

            auto vbView = item.mesh->GetVertexBuffer().GetView();
            cmdList->IASetVertexBuffers(0, 1, &vbView);
            auto ibView = item.mesh->GetIndexBuffer().GetView();
            cmdList->IASetIndexBuffer(&ibView);
            cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);
        }
    }

    // Skinned mesh outlines: upload bone matrices to fresh slots
    if (!outlineSkinnedItems.empty() && boneMatrixPairBuffer_) {
        cmdList->SetPipelineState(outlinePipeline_.GetSkinnedPSO());
        cmdList->SetGraphicsRootSignature(outlinePipeline_.GetSkinnedRootSignature());

        BoneMatrixPair* mappedBoneData = nullptr;
        boneMatrixPairBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBoneData));

        for (const auto& item : outlineSkinnedItems) {
            if (!item.mesh) continue;
            if (!item.boneMatrixPairs || item.boneMatrixPairs->empty()) continue;
            if (currentBoneSlot_ >= MAX_SKINNED_OBJECTS) break;

            size_t numBones = (std::min)(item.boneMatrixPairs->size(), static_cast<size_t>(MAX_BONES));
            BoneMatrixPair* slotData = mappedBoneData + (currentBoneSlot_ * MAX_BONES);
            for (size_t j = 0; j < numBones; ++j) {
                const auto& pair = (*item.boneMatrixPairs)[j];
                Matrix4x4 transposedSkeleton = pair.skeletonSpaceMatrix.Transpose();
                Matrix4x4 transposedInvTranspose = pair.skeletonSpaceInverseTransposeMatrix.Transpose();
                transposedSkeleton.ToFloatArray(reinterpret_cast<float*>(&slotData[j].skeletonSpaceMatrix));
                transposedInvTranspose.ToFloatArray(reinterpret_cast<float*>(&slotData[j].skeletonSpaceInverseTransposeMatrix));
            }

            TransformCB transformData;
            auto mvp = item.worldMatrix * viewMatrix * projection;
            StoreTransposedMatrix(transformData.world, item.worldMatrix);
            StoreTransposedMatrix(transformData.view, viewMatrix);
            StoreTransposedMatrix(transformData.projection, projection);
            StoreTransposedMatrix(transformData.mvp, mvp);
            auto transformGpuAddr = skinnedTransformBuffer_.Update(transformData);

            cmdList->SetGraphicsRootConstantBufferView(0, transformGpuAddr);
            cmdList->SetGraphicsRootDescriptorTable(1, boneMatrixPairSRVs_[currentBoneSlot_]);
            cmdList->SetGraphicsRootConstantBufferView(2, outlineGpuAddr);

            auto vbView = item.mesh->GetVertexBuffer().GetView();
            cmdList->IASetVertexBuffers(0, 1, &vbView);
            auto ibView = item.mesh->GetIndexBuffer().GetView();
            cmdList->IASetIndexBuffer(&ibView);
            cmdList->DrawIndexedInstanced(item.mesh->GetIndexBuffer().GetIndexCount(), 1, 0, 0, 0);

            currentBoneSlot_++;
        }

        boneMatrixPairBuffer_->Unmap(0, nullptr);
    }
}

} // namespace UnoEngine
