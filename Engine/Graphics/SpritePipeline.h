#pragma once

#include "D3D12Common.h"
#include "Shader.h"

namespace UnoEngine {

class SpritePipeline {
public:
    SpritePipeline() = default;
    ~SpritePipeline() = default;

    void Initialize(ID3D12Device* device, const Shader& vertexShader, const Shader& pixelShader);

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, const Shader& vertexShader, const Shader& pixelShader);

    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
};

} // namespace UnoEngine
