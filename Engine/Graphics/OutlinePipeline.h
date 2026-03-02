#pragma once

#include "D3D12Common.h"
#include "Shader.h"

namespace UnoEngine {

class GraphicsDevice;

class OutlinePipeline {
public:
    OutlinePipeline() = default;
    ~OutlinePipeline() = default;

    void Initialize(GraphicsDevice* graphics, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);

    ID3D12PipelineState*  GetStaticPSO()            const { return staticPSO_.Get(); }
    ID3D12PipelineState*  GetSkinnedPSO()           const { return skinnedPSO_.Get(); }
    ID3D12RootSignature*  GetStaticRootSignature()  const { return staticRS_.Get(); }
    ID3D12RootSignature*  GetSkinnedRootSignature() const { return skinnedRS_.Get(); }

private:
    void CreateStaticRS(ID3D12Device* device);
    void CreateSkinnedRS(ID3D12Device* device);
    void CreateStaticPSO(ID3D12Device* device, const Shader& vs, const Shader& ps,
                         DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);
    void CreateSkinnedPSO(ID3D12Device* device, const Shader& vs, const Shader& ps,
                          DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);

    ComPtr<ID3D12RootSignature> staticRS_;
    ComPtr<ID3D12RootSignature> skinnedRS_;
    ComPtr<ID3D12PipelineState> staticPSO_;
    ComPtr<ID3D12PipelineState> skinnedPSO_;
};

} // namespace UnoEngine
