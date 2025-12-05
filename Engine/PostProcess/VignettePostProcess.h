#pragma once

#include "PostProcess.h"
#include "PostProcessType.h"
#include "../Graphics/D3D12Common.h"

namespace UnoEngine {

class VignettePostProcess : public PostProcess {
public:
    VignettePostProcess() = default;
    ~VignettePostProcess() override = default;

    void Initialize(GraphicsDevice* graphics) override;
    void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) override;
    PostProcessType GetType() const override { return PostProcessType::Vignette; }
    const char* GetName() const override { return "Vignette"; }

    // パラメータ
    VignetteParams& GetParams() { return m_params; }
    const VignetteParams& GetParams() const { return m_params; }
    void SetParams(const VignetteParams& params) { m_params = params; }

private:
    struct alignas(256) VignetteCB {
        float radius;
        float softness;
        float intensity;
        float padding;
    };

    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, const void* vsBlob, size_t vsSize,
                             const void* psBlob, size_t psSize);
    void CreateConstantBuffer(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12Resource> m_constantBuffer;
    VignetteCB* m_cbMapped = nullptr;

    VignetteParams m_params;
};

} // namespace UnoEngine
