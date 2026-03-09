#include "pch.h"
#include "ShadowPipeline.h"
#include "SkinnedVertex.h"

namespace UnoEngine {

void ShadowPipeline::Initialize(ID3D12Device* device) {
    CreateStaticPipeline(device);
    CreateSkinnedPipeline(device);
}

void ShadowPipeline::CreateStaticPipeline(ID3D12Device* device) {
    // Root param: [0] CBV b0 (ShadowTransformCB)
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace  = 0;
    rootParam.ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters   = &rootParam;
    rsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err),
                  "Shadow static root sig serialize failed");
    ThrowIfFailed(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                  IID_PPV_ARGS(&staticRootSig_)), "Shadow static root sig create failed");

    Shader vs;
    vs.CompileFromFile(L"Shaders/Shadow/ShadowVS.hlsl", ShaderStage::Vertex);

    D3D12_INPUT_ELEMENT_DESC inputElems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature          = staticRootSig_.Get();
    pso.VS                      = vs.GetBytecodeDesc();
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = 0; // no color write
    pso.SampleMask              = UINT_MAX;
    pso.RasterizerState.FillMode      = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode      = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.DepthBias     = 100;
    pso.RasterizerState.SlopeScaledDepthBias = 1.0f;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.DepthStencilState.DepthEnable     = TRUE;
    pso.DepthStencilState.DepthWriteMask  = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc       = D3D12_COMPARISON_FUNC_LESS;
    pso.InputLayout             = { inputElems, _countof(inputElems) };
    pso.PrimitiveTopologyType   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets        = 0;
    pso.DSVFormat               = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count        = 1;

    ThrowIfFailed(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&staticPSO_)),
                  "Shadow static PSO create failed");
}

void ShadowPipeline::CreateSkinnedPipeline(ID3D12Device* device) {
    // Root params: [0] CBV b0 (ShadowTransformCB), [1] DescTable t0 (bones)
    D3D12_DESCRIPTOR_RANGE boneRange = {};
    boneRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    boneRange.NumDescriptors     = 1;
    boneRange.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER rootParams[2] = {};
    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges   = &boneRange;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 2;
    rsDesc.pParameters   = rootParams;
    rsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err),
                  "Shadow skinned root sig serialize failed");
    ThrowIfFailed(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                  IID_PPV_ARGS(&skinnedRootSig_)), "Shadow skinned root sig create failed");

    Shader vs;
    vs.CompileFromFile(L"Shaders/Shadow/SkinnedShadowVS.hlsl", ShaderStage::Vertex);

    D3D12_INPUT_ELEMENT_DESC inputElems[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(SkinnedVertex, px),          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(SkinnedVertex, nx),          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(SkinnedVertex, u),           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, offsetof(SkinnedVertex, boneIndices), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SkinnedVertex, boneWeights), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature          = skinnedRootSig_.Get();
    pso.VS                      = vs.GetBytecodeDesc();
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
    pso.SampleMask              = UINT_MAX;
    pso.RasterizerState.FillMode      = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode      = D3D12_CULL_MODE_FRONT;
    pso.RasterizerState.DepthBias     = 100;
    pso.RasterizerState.SlopeScaledDepthBias = 1.0f;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.DepthStencilState.DepthEnable     = TRUE;
    pso.DepthStencilState.DepthWriteMask  = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc       = D3D12_COMPARISON_FUNC_LESS;
    pso.InputLayout             = { inputElems, _countof(inputElems) };
    pso.PrimitiveTopologyType   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets        = 0;
    pso.DSVFormat               = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count        = 1;

    ThrowIfFailed(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&skinnedPSO_)),
                  "Shadow skinned PSO create failed");
}

} // namespace UnoEngine
