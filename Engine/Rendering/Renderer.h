#pragma once

#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/SkinnedPipeline.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/DynamicConstantBuffer.h"
#include "RenderItem.h"
#include "SkinnedRenderItem.h"
#include "RenderView.h"
#include "LightManager.h"
#include "DebugRenderer.h"
#include "../Window/Window.h"
#include "../UI/ImGuiManager.h"
#include "../Math/MathCommon.h"
#include <vector>

namespace UnoEngine {

struct alignas(256) TransformCB {
    Float4x4 world;
    Float4x4 view;
    Float4x4 projection;
    Float4x4 mvp;
};

struct alignas(256) LightCB {
    Float3 directionalLightDirection;
    float padding0;
    Float3 directionalLightColor;
    float directionalLightIntensity;
    Float3 ambientLight;
    float padding1;
    Float3 cameraPosition;
    float padding2;
};

struct alignas(256) MaterialCB {
    Float3 albedo;
    float metallic;
    float roughness;
    Float3 padding;
};

class Scene;

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    void Initialize(GraphicsDevice* graphics, Window* window);
    void BeginFrame();  // フレーム開始時にダイナミックバッファをリセット
    void Draw(const RenderView& view, const std::vector<RenderItem>& renderItems, LightManager* lightManager, Scene* scene = nullptr);
    void DrawSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items, LightManager* lightManager);
    void DrawToTexture(ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                       D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, const RenderView& view,
                       const std::vector<RenderItem>& items, LightManager* lightManager,
                       const std::vector<SkinnedRenderItem>& skinnedItems = {},
                       bool enableDebugDraw = false);
    void RenderUIOnly(Scene* scene);

    Pipeline* GetPipeline() { return &pipeline_; }
    SkinnedPipeline* GetSkinnedPipeline() { return &skinnedPipeline_; }
    ImGuiManager* GetImGuiManager() { return imguiManager_.get(); }
    DebugRenderer* GetDebugRenderer() { return debugRenderer_.get(); }

protected:
    virtual void RenderUI(Scene* scene);

private:
    void SetupViewport();
    void UpdateLighting(const RenderView& view, LightManager* lightManager);
    void RenderMeshes(const RenderView& view, const std::vector<RenderItem>& items);
    void RenderSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items);
    void CreateBoneMatrixPairBuffer(ID3D12Device* device);

private:
    GraphicsDevice* graphics_ = nullptr;
    Window* window_ = nullptr;
    Pipeline pipeline_;
    SkinnedPipeline skinnedPipeline_;

    // スキンメッシュ用のダイナミックバッファ（フレーム内で複数回更新可能）
    DynamicConstantBuffer<TransformCB> skinnedTransformBuffer_;
    DynamicConstantBuffer<MaterialCB> skinnedMaterialBuffer_;
    
    // 通常メッシュ用（DynamicConstantBufferで複数ビュー対応）
    DynamicConstantBuffer<TransformCB> constantBuffer_;
    DynamicConstantBuffer<LightCB> lightBuffer_;
    DynamicConstantBuffer<MaterialCB> materialBuffer_;
    ConstantBuffer<BoneMatricesCB> boneBuffer_;

    // 現在のライトバッファのGPUアドレス（UpdateLightingで更新）
    D3D12_GPU_VIRTUAL_ADDRESS currentLightGpuAddr_ = 0;
    
    // StructuredBuffer for bone matrices（複数モデル対応のリングバッファ）
    static constexpr uint32 MAX_SKINNED_OBJECTS = 16;  // 1フレームで描画可能な最大スキンモデル数
    ComPtr<ID3D12Resource> boneMatrixPairBuffer_;
    D3D12_GPU_DESCRIPTOR_HANDLE boneMatrixPairSRVs_[MAX_SKINNED_OBJECTS];  // 各スロット用のSRV
    uint32 boneMatrixPairSRVBaseIndex_ = 0;
    uint32 currentBoneSlot_ = 0;  // 現在使用中のスロット

    UniquePtr<ImGuiManager> imguiManager_;
    UniquePtr<DebugRenderer> debugRenderer_;
};

} // namespace UnoEngine
