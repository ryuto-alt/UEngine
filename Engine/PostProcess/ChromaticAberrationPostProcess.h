#pragma once

#include "PostProcess.h"
#include "PostProcessType.h"
#include "../Core/Types.h"
#include <wrl/client.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

namespace UnoEngine {

class GraphicsDevice;
class RenderTexture;

class ChromaticAberrationPostProcess : public PostProcess {
public:
    ChromaticAberrationPostProcess() = default;
    ~ChromaticAberrationPostProcess() override = default;

    void Initialize(GraphicsDevice* graphics) override;
    void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) override;
    PostProcessType GetType() const override { return PostProcessType::ChromaticAberration; }
    const char* GetName() const override { return "ChromaticAberration"; }

    ChromaticAberrationParams& GetParams() { return m_params; }
    const ChromaticAberrationParams& GetParams() const { return m_params; }
    void SetParams(const ChromaticAberrationParams& params) { m_params = params; }

private:
    struct alignas(256) ChromaticAberrationCB {
        float intensity;
        float redOffset;
        float greenOffset;
        float blueOffset;
    };

    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device, const void* vsBlob, size_t vsSize,
                             const void* psBlob, size_t psSize);
    void CreateConstantBuffer(ID3D12Device* device);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12Resource> m_constantBuffer;
    ChromaticAberrationCB* m_cbMapped = nullptr;

    ChromaticAberrationParams m_params;
};

} // namespace UnoEngine
