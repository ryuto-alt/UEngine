#include "pch.h"
#include "InfiniteGridPipeline.h"
#include "../Core/Logger.h"

namespace UnoEngine {

void InfiniteGridPipeline::Initialize(
    ID3D12Device* device,
    const Shader& vertexShader,
    const Shader& pixelShader,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat
) {
    CreateRootSignature(device);
    CreatePipelineState(device, vertexShader, pixelShader, rtvFormat, dsvFormat);
}

void InfiniteGridPipeline::CreateRootSignature(ID3D12Device* device) {
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = &rootParam;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(
        D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
        "Failed to serialize grid root signature"
    );

    ThrowIfFailed(
        device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)),
        "Failed to create grid root signature"
    );
}

void InfiniteGridPipeline::CreatePipelineState(
    ID3D12Device* device,
    const Shader& vertexShader,
    const Shader& pixelShader,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat
) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = vertexShader.GetBytecodeDesc();
    psoDesc.PS = pixelShader.GetBytecodeDesc();

    // ブレンド設定（アルファブレンド）
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // 深度テスト有効、書き込みは無効（グリッドがオブジェクトを隠さないように）
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(
        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_)),
        "Failed to create grid pipeline state"
    );
}

} // namespace UnoEngine
