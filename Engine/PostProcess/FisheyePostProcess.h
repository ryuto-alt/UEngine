#pragma once

#include "PostProcess.h"
#include "PostProcessType.h"
#include "../Graphics/D3D12Common.h"

namespace UnoEngine {

class FisheyePostProcess : public PostProcess {
public:
    FisheyePostProcess() = default;
    ~FisheyePostProcess() override = default;

    void Initialize(GraphicsDevice* graphics) override;
    void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) override;
    PostProcessType GetType() const override { return PostProcessType::Fisheye; }
    const char* GetName() const override { return "Fisheye"; }

    FisheyeParams& GetParams() { return m_params; }
    const FisheyeParams& GetParams() const { return m_params; }
    void SetParams(const FisheyeParams& params) { m_params = params; }

private:
    struct alignas(256) FisheyeCB {
        float strength;
        float zoom;
        float padding[2];
    };

    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, const void* vsBlob, size_t vsSize,
                             const void* psBlob, size_t psSize);
    void CreateConstantBuffer(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12Resource> m_constantBuffer;
    FisheyeCB* m_cbMapped = nullptr;

    FisheyeParams m_params;
};

} // namespace UnoEngine
