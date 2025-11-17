#include "IndexBuffer.h"
#include <cassert>

namespace UnoEngine {

void IndexBuffer::Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                         const uint32* indices, uint32 indexCount) {
    assert(device && commandList && indices && indexCount > 0);

    indexCount_ = indexCount;
    const uint32 bufferSize = sizeof(uint32) * indexCount;

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
        bufferSize,
        1, 1, 1,
        DXGI_FORMAT_UNKNOWN,
        { 1, 0 },
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        D3D12_RESOURCE_FLAG_NONE
    };

    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&buffer_)
    ));

    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer_)
    ));

    void* mappedData = nullptr;
    ThrowIfFailed(uploadBuffer_->Map(0, nullptr, &mappedData));
    memcpy(mappedData, indices, bufferSize);
    uploadBuffer_->Unmap(0, nullptr);

    // COMMON -> COPY_DEST へ遷移
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = buffer_.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barriers[0]);

    commandList->CopyResource(buffer_.Get(), uploadBuffer_.Get());

    // COPY_DEST -> INDEX_BUFFER へ遷移
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = buffer_.Get();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barriers[1]);

    view_.BufferLocation = buffer_->GetGPUVirtualAddress();
    view_.SizeInBytes = bufferSize;
    view_.Format = DXGI_FORMAT_R32_UINT;
}

} // namespace UnoEngine
