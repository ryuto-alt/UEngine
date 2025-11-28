#include "VertexBuffer.h"
#include <cassert>

namespace UnoEngine {

void VertexBuffer::Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                          const void* data, uint32 size, uint32 stride) {
    assert(device && commandList && data && size > 0);

    vertexCount_ = size / stride;

    const D3D12_HEAP_PROPERTIES defaultHeapProps = {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0, 0
    };

    const D3D12_HEAP_PROPERTIES uploadHeapProps = {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0, 0
    };

    const D3D12_RESOURCE_DESC bufferDesc = {
        D3D12_RESOURCE_DIMENSION_BUFFER,
        0,
        size,
        1, 1, 1,
        DXGI_FORMAT_UNKNOWN,
        { 1, 0 },
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        D3D12_RESOURCE_FLAG_NONE
    };

    // GPUメモリにバッファを作成
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&buffer_)
    ), "Failed to create vertex buffer");

    // アップロード用中間バッファを作成
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer_)
    ), "Failed to create vertex upload buffer");

    // データをアップロードバッファにコピー
    void* mappedData = nullptr;
    ThrowIfFailed(uploadBuffer_->Map(0, nullptr, &mappedData));
    memcpy(mappedData, data, size);
    uploadBuffer_->Unmap(0, nullptr);

    // COMMON -> COPY_DEST へ遷移
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = buffer_.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barriers[0]);

    // アップロードバッファからGPUバッファへコピー
    commandList->CopyResource(buffer_.Get(), uploadBuffer_.Get());

    // COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER へ遷移
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = buffer_.Get();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barriers[1]);

    // ビュー作成
    view_.BufferLocation = buffer_->GetGPUVirtualAddress();
    view_.SizeInBytes = size;
    view_.StrideInBytes = stride;
}

} // namespace UnoEngine
