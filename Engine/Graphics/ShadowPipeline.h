#pragma once

#include "D3D12Common.h"
#include "Shader.h"

namespace UnoEngine {

// Depth-only PSO for the shadow map pass
// Root params: [0] CBV b0 (ShadowTransformCB = {world, lightViewProj})
//              [1] DescTable t0 (bone matrices, VERTEX only - skinned variant)
struct alignas(256) ShadowTransformCB {
    Float4x4 world;
    Float4x4 lightViewProj;
    float pad[32]; // pad to 256 bytes: 2*64=128, need 128 more
};

class ShadowPipeline {
public:
    ShadowPipeline() = default;
    ~ShadowPipeline() = default;

    void Initialize(ID3D12Device* device);

    ID3D12RootSignature* GetStaticRootSignature()  const { return staticRootSig_.Get(); }
    ID3D12PipelineState* GetStaticPSO()            const { return staticPSO_.Get(); }
    ID3D12RootSignature* GetSkinnedRootSignature() const { return skinnedRootSig_.Get(); }
    ID3D12PipelineState* GetSkinnedPSO()           const { return skinnedPSO_.Get(); }

private:
    void CreateStaticPipeline(ID3D12Device* device);
    void CreateSkinnedPipeline(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> staticRootSig_;
    ComPtr<ID3D12PipelineState> staticPSO_;
    ComPtr<ID3D12RootSignature> skinnedRootSig_;
    ComPtr<ID3D12PipelineState> skinnedPSO_;
};

} // namespace UnoEngine
