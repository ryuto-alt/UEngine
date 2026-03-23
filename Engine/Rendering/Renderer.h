#pragma once

#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/SkinnedPipeline.h"
#include "../Graphics/OutlinePipeline.h"
#include "../Graphics/ShadowMap.h"
#include "../Graphics/ShadowPipeline.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/DynamicConstantBuffer.h"
#include "../Vegetation/GrassSystem.h"
#include "../Vegetation/GrassRenderer.h"
#include "RenderItem.h"
#include "SkinnedRenderItem.h"
#include "RenderView.h"
#include "LightManager.h"
#include "DebugRenderer.h"
#include "../Window/Window.h"
#include "../UI/ImGuiManager.h"
#include "../Math/MathCommon.h"
#include <vector>
#include <span>
#include <string_view>

namespace UnoEngine {

struct alignas(256) OutlineParamsCB {
    float width = 0.03f;
    float r = 1.0f;
    float g = 0.5f;
    float b = 0.0f;
    float pad[60];
};

static constexpr int MAX_SPOT_SHADOWS = 4;

struct alignas(256) TransformCB {
    Float4x4 world;
    Float4x4 view;
    Float4x4 projection;
    Float4x4 mvp;
    Float4x4 lightViewProj;                    // directional shadow
    Float4x4 spotLightViewProj[MAX_SPOT_SHADOWS]; // spot shadows
};

struct GPUPointLightCB {
    Float3  position;
    float   range;
    Float3  color;
    float   intensity;
};

struct GPUSpotLightCB {
    Float3  position;
    float   range;
    Float3  direction;
    float   spotAngle;
    Float3  color;
    float   intensity;
    float   innerAngle;
    float   pad[3];
};

struct alignas(256) LightCB {
    // Directional
    Float3 directionalLightDirection; float padding0;
    Float3 directionalLightColor;     float directionalLightIntensity;
    Float3 ambientLight;              float padding1;
    Float3 cameraPosition;            float padding2;
    // Point lights (max 8)
    GPUPointLightCB pointLights[8];
    int32_t pointLightCount;          float pad3[3];
    // Spot lights (max 4)
    GPUSpotLightCB  spotLights[4];
    int32_t spotLightCount;           float pad4[3];
    // Shadow
    float shadowBias;
    int32_t spotShadowCount;
    float shadowPad[2];
};

struct alignas(256) MaterialCB {
    Float3 albedo;
    float metallic;
    float roughness;
    float alphaClipThreshold;  // 0 = opaque, >0 = alpha test
    float doubleSided;         // 1.0 = 両面描画（裏面法線反転）
    float useAlphaBlend;       // 1.0 = alpha blending mode (output alpha, no discard)
};

class Scene;

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    void Initialize(GraphicsDevice* graphics, Window* window);
    void BeginFrame();
    void Draw(const RenderView& view, const std::vector<RenderItem>& renderItems, LightManager* lightManager, Scene* scene = nullptr);
    void DrawSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items, LightManager* lightManager);
    // シャドウマップを1回だけ描画（エディタモードで複数ビュー共有用）
    void RenderShadowPrePass(const RenderView& view,
                             const std::vector<RenderItem>& items,
                             const std::vector<SkinnedRenderItem>& skinnedItems,
                             LightManager* lightManager);

    // シャドウマップをDEPTH_WRITEに復元（RenderShadowPrePass使用時、全DrawToTexture完了後に呼ぶ）
    void RestoreShadowMaps();

    void DrawToTexture(ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                       D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, const RenderView& view,
                       const std::vector<RenderItem>& items, LightManager* lightManager,
                       const std::vector<SkinnedRenderItem>& skinnedItems = {},
                       bool enableDebugDraw = false,
                       std::span<const RenderItem> outlineItems = {},
                       std::span<const SkinnedRenderItem> outlineSkinnedItems = {},
                       bool shadowsAlreadyRendered = false);
    void RenderUIOnly(Scene* scene);

#ifdef WITH_EDITOR
    // 起動時ローディング画面（ImGui全画面オーバーレイ）
    void RenderLoadingScreen(std::string_view message, float progress);
#endif

    Pipeline* GetPipeline() { return &pipeline_; }
    SkinnedPipeline* GetSkinnedPipeline() { return &skinnedPipeline_; }
    ImGuiManager* GetImGuiManager() { return imguiManager_.get(); }
    DebugRenderer* GetDebugRenderer() { return debugRenderer_.get(); }
    GrassSystem* GetGrassSystem() { return &grassSystem_; }
    GrassRenderer* GetGrassRenderer() { return &grassRenderer_; }

protected:
    virtual void RenderUI(Scene* scene);

private:
    void SetupViewport();
    void UpdateLighting(const RenderView& view, LightManager* lightManager, Matrix4x4& outLightViewProj);
    void RenderShadowMap(const std::vector<RenderItem>& items, const std::vector<SkinnedRenderItem>& skinnedItems, const Matrix4x4& lightViewProj);
    void RenderSpotShadowMaps(const std::vector<RenderItem>& items, const std::vector<SkinnedRenderItem>& skinnedItems);
    void RenderMeshes(const RenderView& view, const std::vector<RenderItem>& items, const Matrix4x4& lightViewProj);
    void RenderSkinnedMeshes(const RenderView& view, const std::vector<SkinnedRenderItem>& items, const Matrix4x4& lightViewProj);
    void RenderOutline(const RenderView& view,
                       std::span<const RenderItem> outlineItems,
                       std::span<const SkinnedRenderItem> outlineSkinnedItems);
    void CreateBoneMatrixPairBuffer(ID3D12Device* device);

private:
    GraphicsDevice* graphics_ = nullptr;
    Window* window_ = nullptr;
    Pipeline pipeline_;
    SkinnedPipeline skinnedPipeline_;
    OutlinePipeline outlinePipeline_;
    ShadowMap shadowMap_;
    ShadowMap spotShadowMaps_[MAX_SPOT_SHADOWS];
    ShadowPipeline shadowPipeline_;

    DynamicConstantBuffer<TransformCB> skinnedTransformBuffer_;
    DynamicConstantBuffer<MaterialCB> skinnedMaterialBuffer_;

    DynamicConstantBuffer<TransformCB> constantBuffer_;
    DynamicConstantBuffer<LightCB> lightBuffer_;
    DynamicConstantBuffer<MaterialCB> materialBuffer_;
    ConstantBuffer<BoneMatricesCB> boneBuffer_;
    DynamicConstantBuffer<OutlineParamsCB> outlineCB_;

    D3D12_GPU_VIRTUAL_ADDRESS currentLightGpuAddr_ = 0;

    static constexpr uint32 MAX_SKINNED_OBJECTS = 16;
    ComPtr<ID3D12Resource> boneMatrixPairBuffer_;
    D3D12_GPU_DESCRIPTOR_HANDLE boneMatrixPairSRVs_[MAX_SKINNED_OBJECTS];
    uint32 boneMatrixPairSRVBaseIndex_ = 0;
    uint32 currentBoneSlot_ = 0;

    DynamicConstantBuffer<ShadowTransformCB> shadowTransformBuffer_;

    UniquePtr<ImGuiManager> imguiManager_;
    UniquePtr<DebugRenderer> debugRenderer_;

    GrassSystem grassSystem_;
    GrassRenderer grassRenderer_;

    Matrix4x4 lastLightViewProj_;
    Matrix4x4 spotLightViewProjs_[MAX_SPOT_SHADOWS];
    int32_t activeSpotShadowCount_ = 0;
};

} // namespace UnoEngine
