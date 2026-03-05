#pragma once

#include "../Graphics/D3D12Common.h"
#include "../Graphics/Shader.h"

namespace UnoEngine {

// 草専用パイプラインステート
// - インスタンシング対応の入力レイアウト（Slot0=頂点, Slot1=インスタンス）
// - カリングなし（両面描画）
// - アルファテスト
class GrassPipeline {
public:
    GrassPipeline() = default;
    ~GrassPipeline() = default;

    void Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat);

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, DXGI_FORMAT rtvFormat);

    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
    Shader vertexShader_;
    Shader pixelShader_;
};

} // namespace UnoEngine
