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
#include <DirectXMath.h>
#include <vector>

namespace UnoEngine {

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

class Scene;

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    void Initialize(GraphicsDevice* graphics, Window* window);
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
    
    // 通常メッシュ用（互換性のため保持）
    ConstantBuffer<TransformCB> constantBuffer_;
    ConstantBuffer<LightCB> lightBuffer_;
    ConstantBuffer<MaterialCB> materialBuffer_;
    ConstantBuffer<BoneMatricesCB> boneBuffer_;
    
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
