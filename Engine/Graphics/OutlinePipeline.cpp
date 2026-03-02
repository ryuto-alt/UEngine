#include "pch.h"
#include "OutlinePipeline.h"
#include "GraphicsDevice.h"
#include "SkinnedVertex.h"

namespace UnoEngine {

void OutlinePipeline::Initialize(GraphicsDevice* graphics, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat) {
    auto* device = graphics->GetDevice();

    Shader staticVS;
    staticVS.CompileFromFile(L"Shaders/Outline/OutlineVS.hlsl", ShaderStage::Vertex);

    Shader skinnedVS;
    skinnedVS.CompileFromFile(L"Shaders/Outline/SkinnedOutlineVS.hlsl", ShaderStage::Vertex);

    Shader ps;
    ps.CompileFromFile(L"Shaders/Outline/OutlinePS.hlsl", ShaderStage::Pixel);

    CreateStaticRS(device);
    CreateSkinnedRS(device);
    CreateStaticPSO(device, staticVS, ps, rtvFormat, dsvFormat);
    CreateSkinnedPSO(device, skinnedVS, ps, rtvFormat, dsvFormat);
}

void OutlinePipeline::CreateStaticRS(ID3D12Device* device) {
    // [0] b0 - Transform (VS)
    // [1] b1 - OutlineParams (VS+PS)
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].Descriptor.ShaderRegister = 1;
    rootParams[1].Descriptor.RegisterSpace = 0;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 2;
    desc.pParameters = rootParams;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err),
        "Failed to serialize static outline root signature"
    );
    ThrowIfFailed(
        device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                                    IID_PPV_ARGS(&staticRS_)),
        "Failed to create static outline root signature"
    );
}

void OutlinePipeline::CreateSkinnedRS(ID3D12Device* device) {
    // [0] b0 - Transform (VS)
    // [1] DescriptorTable(t0) - BoneMatrixPair StructuredBuffer (VS)
    // [2] b1 - OutlineParams (VS+PS)
    D3D12_DESCRIPTOR_RANGE boneRange = {};
    boneRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    boneRange.NumDescriptors = 1;
    boneRange.BaseShaderRegister = 0;
    boneRange.RegisterSpace = 0;
    boneRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[3] = {};

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &boneRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[2].Descriptor.ShaderRegister = 1;
    rootParams[2].Descriptor.RegisterSpace = 0;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 3;
    desc.pParameters = rootParams;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err),
        "Failed to serialize skinned outline root signature"
    );
    ThrowIfFailed(
        device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                                    IID_PPV_ARGS(&skinnedRS_)),
        "Failed to create skinned outline root signature"
    );
}

namespace {

D3D12_GRAPHICS_PIPELINE_STATE_DESC MakeOutlinePSODesc(
    ID3D12RootSignature* rs,
    const Shader& vs,
    const Shader& ps,
    const D3D12_INPUT_ELEMENT_DESC* inputElements,
    uint32_t numElements,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rs;
    psoDesc.VS = vs.GetBytecodeDesc();
    psoDesc.PS = ps.GetBytecodeDesc();

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

    // Backface shell: cull front faces so only the expanded back faces are visible
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = 0;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // No depth write so the outline doesn't occlude the mesh in subsequent frames
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    psoDesc.InputLayout = { inputElements, numElements };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = 1;
    return psoDesc;
}

} // anonymous namespace

void OutlinePipeline::CreateStaticPSO(ID3D12Device* device, const Shader& vs, const Shader& ps,
                                      DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat) {
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto psoDesc = MakeOutlinePSODesc(staticRS_.Get(), vs, ps,
                                     inputElements, _countof(inputElements),
                                     rtvFormat, dsvFormat);
    ThrowIfFailed(
        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&staticPSO_)),
        "Failed to create static outline PSO"
    );
}

void OutlinePipeline::CreateSkinnedPSO(ID3D12Device* device, const Shader& vs, const Shader& ps,
                                       DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat) {
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(SkinnedVertex, px),          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(SkinnedVertex, nx),          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(SkinnedVertex, u),           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, offsetof(SkinnedVertex, boneIndices), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SkinnedVertex, boneWeights), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto psoDesc = MakeOutlinePSODesc(skinnedRS_.Get(), vs, ps,
                                     inputElements, _countof(inputElements),
                                     rtvFormat, dsvFormat);
    ThrowIfFailed(
        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&skinnedPSO_)),
        "Failed to create skinned outline PSO"
    );
}

} // namespace UnoEngine
