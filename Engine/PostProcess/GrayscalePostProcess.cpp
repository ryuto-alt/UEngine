#include "pch.h"
#include "GrayscalePostProcess.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/RenderTexture.h"
#include "../Graphics/Shader.h"
#include "../Graphics/d3dx12.h"

namespace UnoEngine {

void GrayscalePostProcess::Initialize(GraphicsDevice* graphics) {
    // シェーダーコンパイル
    Shader vs;
    vs.CompileFromFile(L"Shaders/PostProcess/FullscreenVS.hlsl", ShaderStage::Vertex);

    Shader ps;
    ps.CompileFromFile(L"Shaders/PostProcess/GrayscalePS.hlsl", ShaderStage::Pixel);

    m_pipeline.Initialize(graphics->GetDevice(),
                          vs.GetBytecode()->GetBufferPointer(), vs.GetBytecode()->GetBufferSize(),
                          ps.GetBytecode()->GetBufferPointer(), ps.GetBytecode()->GetBufferSize());
}

void GrayscalePostProcess::Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) {
    if (!m_enabled) return;

    auto* cmdList = graphics->GetCommandList();
    auto* heap = graphics->GetSRVHeap();

    // Transition destination to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = destination->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Set render target
    auto rtvHandle = destination->GetRTVHandle();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Set viewport
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(destination->GetWidth());
    viewport.Height = static_cast<float>(destination->GetHeight());
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.right = destination->GetWidth();
    scissorRect.bottom = destination->GetHeight();

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    // Set pipeline
    cmdList->SetPipelineState(m_pipeline.GetPipelineState());
    cmdList->SetGraphicsRootSignature(m_pipeline.GetRootSignature());

    // Set descriptor heap and bind source texture
    ID3D12DescriptorHeap* heaps[] = {heap};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(0, source->GetSRVHandle());

    // Draw fullscreen triangle
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

    // Transition destination back to shader resource
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}

} // namespace UnoEngine
