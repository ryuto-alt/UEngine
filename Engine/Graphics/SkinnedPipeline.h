#pragma once

#include "D3D12Common.h"
#include "Shader.h"
#include "../Animation/Skeleton.h"
#include "../Math/MathCommon.h"

namespace UnoEngine {

// ボーン行列用定数バッファ
struct alignas(256) BoneMatricesCB {
    Float4x4 bones[MAX_BONES];
};

// スキンメッシュ用パイプライン
class SkinnedPipeline {
public:
    SkinnedPipeline() = default;
    ~SkinnedPipeline() = default;

    void Initialize(
        ID3D12Device* device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        DXGI_FORMAT rtvFormat
    );

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
