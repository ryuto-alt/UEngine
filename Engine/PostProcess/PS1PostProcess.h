#pragma once

#include "PostProcess.h"
#include "PostProcessType.h"
#include "../Graphics/D3D12Common.h"

namespace UnoEngine {

class PS1PostProcess : public PostProcess {
public:
    PS1PostProcess() = default;
    ~PS1PostProcess() override = default;

    void Initialize(GraphicsDevice* graphics) override;
    void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) override;
    PostProcessType GetType() const override { return PostProcessType::PS1; }
    const char* GetName() const override { return "PS1"; }

    PS1Params& GetParams() { return m_params; }
    const PS1Params& GetParams() const { return m_params; }
    void SetParams(const PS1Params& params) { m_params = params; }

private:
    struct alignas(256) PS1CB {
        float colorDepth;
        float resolutionScale;
        float ditherEnabled;
        float ditherStrength;
        float screenWidth;
        float screenHeight;
        float padding[2];
    };

    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, const void* vsBlob, size_t vsSize,
                             const void* psBlob, size_t psSize);
    void CreateConstantBuffer(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12Resource> m_constantBuffer;
    PS1CB* m_cbMapped = nullptr;

    PS1Params m_params;
};

} // namespace UnoEngine