#include "pch.h"
#include "GrassPipeline.h"

namespace UnoEngine {

void GrassPipeline::Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat) {
    vertexShader_.CompileFromFile(L"Shaders/Grass/GrassVS.hlsl", ShaderStage::Vertex);
    pixelShader_.CompileFromFile(L"Shaders/Grass/GrassPS.hlsl", ShaderStage::Pixel);

    CreateRootSignature(device);
    CreatePipelineState(device, rtvFormat);
}

void GrassPipeline::CreateRootSignature(ID3D12Device* device) {
    // [0] CBV: GrassTransformCB (b0, ALL) - view/proj/time/wind/colors/etc.
    // [1] Table: Grass albedo texture (t0, PIXEL)
    // [2] CBV: LightCB (b1, PIXEL)
    // [3] Table: Shadow map (t1, PIXEL)
    // [4] Table: Spot shadow maps (t2-t5, PIXEL)
    // [5] Table: Noise textures (t6-t8, ALL) - color variation 1&2, wind noise

    D3D12_DESCRIPTOR_RANGE albedoRange = {};
    albedoRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    albedoRange.NumDescriptors = 1;
    albedoRange.BaseShaderRegister = 0;
    albedoRange.RegisterSpace = 0;
    albedoRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE shadowRange = {};
    shadowRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    shadowRange.NumDescriptors = 1;
    shadowRange.BaseShaderRegister = 1;
    shadowRange.RegisterSpace = 0;
    shadowRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE spotShadowRange = {};
    spotShadowRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    spotShadowRange.NumDescriptors = 4;
    spotShadowRange.BaseShaderRegister = 2;
    spotShadowRange.RegisterSpace = 0;
    spotShadowRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE noiseRange = {};
    noiseRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    noiseRange.NumDescriptors = 3;
    noiseRange.BaseShaderRegister = 6;  // t6, t7, t8
    noiseRange.RegisterSpace = 0;
    noiseRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[6] = {};

    // [0] GrassTransformCB
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [1] Albedo texture
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &albedoRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // [2] LightCB
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[2].Descriptor.ShaderRegister = 1;
    rootParams[2].Descriptor.RegisterSpace = 0;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // [3] Shadow map
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[3].DescriptorTable.pDescriptorRanges = &shadowRange;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // [4] Spot shadow maps
    rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[4].DescriptorTable.pDescriptorRanges = &spotShadowRange;
    rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // [5] Noise textures (VS + PS 両方で使う)
    rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[5].DescriptorTable.pDescriptorRanges = &noiseRange;
    rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // サンプラー
    D3D12_STATIC_SAMPLER_DESC samplers[3] = {};

    // s0: アルベドサンプラー（Clamp, Anisotropic）
    auto& s0 = samplers[0];
    s0.Filter = D3D12_FILTER_ANISOTROPIC;
    s0.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s0.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s0.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s0.MipLODBias = 0.0f;
    s0.MaxAnisotropy = 8;
    s0.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    s0.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    s0.MinLOD = 0.0f;
    s0.MaxLOD = D3D12_FLOAT32_MAX;
    s0.ShaderRegister = 0;
    s0.RegisterSpace = 0;
    s0.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1: シャドウ比較サンプラー
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

    // s2: ノイズサンプラー（Wrap, Linear, VS+PS両方で使用）
    auto& s2 = samplers[2];
    s2.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    s2.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s2.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s2.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s2.MipLODBias = 0.0f;
    s2.MaxAnisotropy = 1;
    s2.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    s2.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    s2.MinLOD = 0.0f;
    s2.MaxLOD = D3D12_FLOAT32_MAX;
    s2.ShaderRegister = 2;
    s2.RegisterSpace = 0;
    s2.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 6;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 3;
    rootSigDesc.pStaticSamplers = samplers;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(
        D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
        "Failed to serialize grass root signature");

    ThrowIfFailed(
        device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)),
        "Failed to create grass root signature");
}

void GrassPipeline::CreatePipelineState(ID3D12Device* device, DXGI_FORMAT rtvFormat) {
    // 入力レイアウト: Slot0 = 草頂点, Slot1 = インスタンスデータ
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        // Slot 0: Per-vertex data (GrassVertex)
        { "POSITION",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",      0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "HEIGHT_FACTOR", 0, DXGI_FORMAT_R32_FLOAT,       0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

        // Slot 1: Per-instance data (GrassInstance)
        { "INSTANCE_POS",  0, DXGI_FORMAT_R32G32B32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_DATA", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 12, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = vertexShader_.GetBytecodeDesc();
    psoDesc.PS = pixelShader_.GetBytecodeDesc();

    // ブレンドステート（不透明、アルファテストはシェーダーのdiscardで処理）
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

    // ラスタライザ: カリングなし（草は両面描画）
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = 0;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // 深度ステンシル
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
        "Failed to create grass pipeline state");
}

} // namespace UnoEngine
