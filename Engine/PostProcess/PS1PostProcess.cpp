#include "pch.h"
#include "PS1PostProcess.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/RenderTexture.h"
#include "../Graphics/Shader.h"
#include "../Graphics/d3dx12.h"

namespace UnoEngine {

void PS1PostProcess::Initialize(GraphicsDevice* graphics) {
    auto* device = graphics->GetDevice();

    Shader vs;
    vs.CompileFromFile(L"Shaders/PostProcess/FullscreenVS.hlsl", ShaderStage::Vertex);

    Shader ps;
    ps.CompileFromFile(L"Shaders/PostProcess/PS1PS.hlsl", ShaderStage::Pixel);

    CreateRootSignature(device);
    CreatePipelineState(device,
                        vs.GetBytecode()->GetBufferPointer(), vs.GetBytecode()->GetBufferSize(),
                        ps.GetBytecode()->GetBufferPointer(), ps.GetBytecode()->GetBufferSize());
    CreateConstantBuffer(device);
}

void PS1PostProcess::CreateRootSignature(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE1 srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParams[2];
    rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
                                           D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Point sampler for pixelated look
    D3D12_STATIC_SAMPLER_DESC pointSampler = {};
    pointSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    pointSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    pointSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    pointSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    pointSampler.MipLODBias = 0;
    pointSampler.MaxAnisotropy = 0;
    pointSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    pointSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    pointSampler.MinLOD = 0.0f;
    pointSampler.MaxLOD = D3D12_FLOAT32_MAX;
    pointSampler.ShaderRegister = 0;
    pointSampler.RegisterSpace = 0;
    pointSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &pointSampler,
                          D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    D3DX12SerializeVersionedRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
                                           &signature, &error);

    device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                 IID_PPV_ARGS(&m_rootSignature));
}

void PS1PostProcess::CreatePipelineState(ID3D12Device* device, const void* vsBlob, size_t vsSize,
                                          const void* psBlob, size_t psSize) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {vsBlob, vsSize};
    psoDesc.PS = {psBlob, psSize};
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.SampleDesc.Count = 1;

    device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
}

void PS1PostProcess::CreateConstantBuffer(ID3D12Device* device) {
    constexpr uint32 bufferSize = sizeof(PS1CB);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                    &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                    nullptr, IID_PPV_ARGS(&m_constantBuffer));

    CD3DX12_RANGE readRange(0, 0);
    m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbMapped));
}

void PS1PostProcess::Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) {
    if (!m_enabled) return;

    // Update constant buffer
    m_cbMapped->colorDepth = static_cast<float>(m_params.colorDepth);
    m_cbMapped->resolutionScale = m_params.resolutionScale;
    m_cbMapped->ditherEnabled = m_params.ditherEnabled ? 1.0f : 0.0f;
    m_cbMapped->ditherStrength = m_params.ditherStrength;
    m_cbMapped->screenWidth = static_cast<float>(destination->GetWidth());
    m_cbMapped->screenHeight = static_cast<float>(destination->GetHeight());

    auto* cmdList = graphics->GetCommandList();
    auto* heap = graphics->GetSRVHeap();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = destination->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    auto rtvHandle = destination->GetRTVHandle();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(destination->GetWidth());
    viewport.Height = static_cast<float>(destination->GetHeight());
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.right = destination->GetWidth();
    scissorRect.bottom = destination->GetHeight();

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, source->GetSRVHandle());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}

} // namespace UnoEngine