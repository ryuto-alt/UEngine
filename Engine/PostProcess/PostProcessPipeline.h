#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "../Graphics/D3D12Common.h"

namespace UnoEngine {

class GraphicsDevice;

// ポストプロセス用のフルスクリーンクアッド描画パイプライン
class PostProcessPipeline : public NonCopyable {
public:
    PostProcessPipeline() = default;
    ~PostProcessPipeline() = default;

    void Initialize(ID3D12Device* device, const void* vsBlob, size_t vsSize,
                    const void* psBlob, size_t psSize);

    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return m_pipelineState.Get(); }

private:
    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, const void* vsBlob, size_t vsSize,
                             const void* psBlob, size_t psSize);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
};

} // namespace UnoEngine
