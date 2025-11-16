#pragma once

#include "D3D12Common.h"
#include "Shader.h"

namespace UnoEngine {

// 頂点レイアウト定義
struct VertexLayout {
    static constexpr D3D12_INPUT_ELEMENT_DESC GetDefaultLayout() {
        return { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    }
};

// パイプラインステート管理
class Pipeline {
public:
    Pipeline() = default;
    ~Pipeline() = default;

    // 初期化
    void Initialize(
        ID3D12Device* device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat
    );

    // アクセサ
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(
        ID3D12Device* device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat
    );

private:
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
};

} // namespace UnoEngine
