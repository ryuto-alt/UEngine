#include "pch.h"
#include "Pipeline.h"

namespace UnoEngine {

void Pipeline::Initialize(
    ID3D12Device* device,
    const Shader& vertexShader,
    const Shader& pixelShader,
    DXGI_FORMAT rtvFormat
) {
    CreateRootSignature(device);
    CreatePipelineState(device, vertexShader, pixelShader, rtvFormat);
}

void Pipeline::CreateRootSignature(ID3D12Device* device) {
    // ディスクリプタレンジ: アルベドテクスチャ (t0)
    D3D12_DESCRIPTOR_RANGE albedoRange = {};
    albedoRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    albedoRange.NumDescriptors = 1;
    albedoRange.BaseShaderRegister = 0;
    albedoRange.RegisterSpace = 0;
    albedoRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ディスクリプタレンジ: シャドウマップ (t1)
    D3D12_DESCRIPTOR_RANGE shadowRange = {};
    shadowRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    shadowRange.NumDescriptors = 1;
    shadowRange.BaseShaderRegister = 1;
    shadowRange.RegisterSpace = 0;
    shadowRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートパラメータ [0]=Transform(b0,ALL) [1]=albedo(t0,PIXEL) [2]=Light(b1,PIXEL)
    //                 [3]=Material(b2,PIXEL) [4]=shadowMap(t1,PIXEL)
    D3D12_ROOT_PARAMETER rootParams[5] = {};

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &albedoRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[2].Descriptor.ShaderRegister = 1;
    rootParams[2].Descriptor.RegisterSpace = 0;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[3].Descriptor.ShaderRegister = 2;
    rootParams[3].Descriptor.RegisterSpace = 0;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[4].DescriptorTable.pDescriptorRanges = &shadowRange;
    rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s0: アルベドサンプラー (異方性)
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
    auto& s0 = samplers[0];
    s0.Filter = D3D12_FILTER_ANISOTROPIC;
    s0.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s0.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s0.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s0.MipLODBias = -0.5f;
    s0.MaxAnisotropy = 16;
    s0.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    s0.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    s0.MinLOD = 0.0f;
    s0.MaxLOD = D3D12_FLOAT32_MAX;
    s0.ShaderRegister = 0;
    s0.RegisterSpace = 0;
    s0.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1: シャドウ比較サンプラー (PCF)
    auto& s1 = samplers[1];
    s1.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    s1.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s1.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s1.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s1.MipLODBias = 0.0f;
    s1.MaxAnisotropy = 1;
    s1.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    s1.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    s1.MinLOD = 0.0f;
    s1.MaxLOD = 0.0f;
    s1.ShaderRegister = 1;
    s1.RegisterSpace = 0;
    s1.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 5;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 2;
    rootSigDesc.pStaticSamplers = samplers;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(
        D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
        "Failed to serialize root signature"
    );

    ThrowIfFailed(
        device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)
        ),
        "Failed to create root signature"
    );
}

void Pipeline::CreatePipelineState(
    ID3D12Device* device,
    const Shader& vertexShader,
    const Shader& pixelShader,
    DXGI_FORMAT rtvFormat
) {
    // 入力レイアウト (position: Vector3, normal: Vector3, uv: Vector2)
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // パイプラインステート記述
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = vertexShader.GetBytecodeDesc();
    psoDesc.PS = pixelShader.GetBytecodeDesc();
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = 0;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(
        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_)),
        "Failed to create pipeline state"
    );
}

} // namespace UnoEngine
