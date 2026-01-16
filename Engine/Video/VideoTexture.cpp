#include "pch.h"
#include "VideoTexture.h"
#include "../Graphics/GraphicsDevice.h"

namespace UnoEngine {

VideoTexture::~VideoTexture() {
    Release();
}

void VideoTexture::Create(GraphicsDevice* graphics, uint32 width, uint32 height, uint32 srvIndex) {
    m_graphics = graphics;
    m_width = width;
    m_height = height;
    m_srvIndex = srvIndex;
    m_isFirstUpload = true;

    auto device = graphics->GetDevice();

    // テクスチャリソース作成（DEFAULT heap, COPY_DEST初期状態）
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture));

    if (FAILED(hr)) {
        return;
    }

    // アップロードバッファ作成（リングバッファ）
    uint64 uploadSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = uploadSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (uint32 i = 0; i < BACK_BUFFER_COUNT; ++i) {
        hr = device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_uploadBuffers[i]));

        if (FAILED(hr)) {
            Release();
            return;
        }
    }

    // SRV作成
    graphics->CreateSRV(m_texture.Get(), srvIndex);

    auto srvHeap = graphics->GetSRVHeap();
    uint32 incrementSize = graphics->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_srvHandle.ptr = srvHeap->GetGPUDescriptorHandleForHeapStart().ptr + srvIndex * incrementSize;

    // ステージングバッファを確保（RGBA 4bytes per pixel）
    m_stagingBuffer.resize(width * height * 4);
}

void VideoTexture::Release() {
    m_texture.Reset();
    for (auto& buffer : m_uploadBuffers) {
        buffer.Reset();
    }
    m_stagingBuffer.clear();
    m_width = 0;
    m_height = 0;
    m_hasPendingFrame = false;
}

void VideoTexture::StageFrame(const void* rgbaData, uint32 rowPitch) {
    if (!rgbaData || m_stagingBuffer.empty()) {
        return;
    }

    // CPUバッファにコピー
    const uint8* src = static_cast<const uint8*>(rgbaData);
    uint32 dstRowPitch = m_width * 4;

    for (uint32 y = 0; y < m_height; ++y) {
        memcpy(
            m_stagingBuffer.data() + y * dstRowPitch,
            src + y * rowPitch,
            dstRowPitch);
    }

    m_stagingRowPitch = dstRowPitch;
    m_hasPendingFrame = true;
}

void VideoTexture::UploadStagedFrame(ID3D12GraphicsCommandList* commandList) {
    if (!m_texture || !m_hasPendingFrame || !commandList) {
        return;
    }

    auto& uploadBuffer = m_uploadBuffers[m_currentBuffer];
    m_currentBuffer = (m_currentBuffer + 1) % BACK_BUFFER_COUNT;

    // アップロードバッファにデータコピー
    D3D12_RESOURCE_DESC texDesc = m_texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    uint32 numRows = 0;
    uint64 rowSizeInBytes = 0;
    uint64 totalBytes = 0;

    m_graphics->GetDevice()->GetCopyableFootprints(
        &texDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    uint8* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr)) {
        return;
    }

    for (uint32 row = 0; row < numRows; ++row) {
        memcpy(
            mappedData + footprint.Offset + row * footprint.Footprint.RowPitch,
            m_stagingBuffer.data() + row * m_stagingRowPitch,
            std::min(static_cast<uint64>(m_stagingRowPitch), rowSizeInBytes));
    }

    uploadBuffer->Unmap(0, nullptr);

    // リソースバリア: 初回はCOPY_DEST、以降はPIXEL_SHADER_RESOURCEから
    if (!m_isFirstUpload) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_texture.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
    }

    // コピー実行
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = uploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;

    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // リソースバリア: COPY_DEST → PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    m_isFirstUpload = false;
    m_hasPendingFrame = false;
}

} // namespace UnoEngine
