#pragma once

#include "../Engine/Graphics/GraphicsDevice.h"
#include "../Engine/Graphics/Pipeline.h"
#include "../Engine/Graphics/ConstantBuffer.h"
#include "../Engine/Graphics/RenderItem.h"
#include "../Engine/Graphics/RenderView.h"
#include "../Engine/Graphics/LightManager.h"
#include "../Engine/Window/Window.h"
#include "../Engine/UI/ImGuiManager.h"
#include <DirectXMath.h>
#include <vector>

namespace UnoEngine {

// Constant buffer structures
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

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    void Initialize(GraphicsDevice* graphics, Window* window);
    void Draw(const RenderView& view, const std::vector<RenderItem>& renderItems, LightManager* lightManager);
    
    Pipeline* GetPipeline() { return &pipeline_; }

private:
    void SetupViewport();
    void SetupPipelineState();
    void RenderUI();

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
