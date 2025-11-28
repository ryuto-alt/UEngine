#pragma once

#include "D3D12Common.h"
#include "Shader.h"

namespace UnoEngine {

class InfiniteGridPipeline {
public:
    InfiniteGridPipeline() = default;
    ~InfiniteGridPipeline() = default;

    void Initialize(
        ID3D12Device* device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT
    );

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(
        ID3D12Device* device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat
    );

    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
};

} // namespace UnoEngine
