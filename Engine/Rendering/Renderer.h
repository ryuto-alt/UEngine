#pragma once

#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Pipeline.h"
#include "../Graphics/ConstantBuffer.h"
#include "RenderItem.h"
#include "RenderView.h"
#include "LightManager.h"
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
    void DrawToTexture(ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, 
                       D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, const RenderView& view, 
                       const std::vector<RenderItem>& items, LightManager* lightManager);

    Pipeline* GetPipeline() { return &pipeline_; }
    ImGuiManager* GetImGuiManager() { return imguiManager_.get(); }

protected:
    virtual void RenderUI(Scene* scene);

private:
    void SetupViewport();
    void UpdateLighting(const RenderView& view, LightManager* lightManager);
    void RenderMeshes(const RenderView& view, const std::vector<RenderItem>& items);

private:
    GraphicsDevice* graphics_ = nullptr;
    Window* window_ = nullptr;
    Pipeline pipeline_;

    ConstantBuffer<TransformCB> constantBuffer_;
    ConstantBuffer<LightCB> lightBuffer_;
    ConstantBuffer<MaterialCB> materialBuffer_;

    UniquePtr<ImGuiManager> imguiManager_;
};

} // namespace UnoEngine
