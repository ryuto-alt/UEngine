#include "RenderTexture.h"
#include "GraphicsDevice.h"

namespace UnoEngine {

void RenderTexture::Create(GraphicsDevice* graphics, uint32 width, uint32 height, uint32 srvIndex) {
    width_ = width;
    height_ = height;
    srvIndex_ = srvIndex;

    auto* device = graphics->GetDevice();

    // RTV用のヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_));

    // レンダーターゲット作成
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.2f;
    clearValue.Color[1] = 0.3f;
    clearValue.Color[2] = 0.4f;
    clearValue.Color[3] = 1.0f;

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&renderTarget_)
    );

    // RTVを作成
    rtvHandle_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(renderTarget_.Get(), nullptr, rtvHandle_);

    // SRVを作成
    graphics->CreateSRV(renderTarget_.Get(), srvIndex);

    // 深度バッファ作成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    
    device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_));
    
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    
    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;
    
    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&depthStencil_)
    );
    
    dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(depthStencil_.Get(), nullptr, dsvHandle_);

    // SRVハンドルを保存
    auto srvHeap = graphics->GetSRVHeap();
    uint32 descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += srvIndex * descriptorSize;

    srvHandle_ = srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandle_.ptr += srvIndex * descriptorSize;
}

void RenderTexture::Resize(GraphicsDevice* graphics, uint32 width, uint32 height) {
    if (width == width_ && height == height_) return;
    if (width == 0 || height == 0) return;

    // GPUの処理完了を待ってからリソースを解放
    graphics->WaitForGPU();

    Release();
    Create(graphics, width, height, srvIndex_);
}

void RenderTexture::Release() {
    renderTarget_.Reset();
    depthStencil_.Reset();
    rtvHeap_.Reset();
    dsvHeap_.Reset();
}

} // namespace UnoEngine
