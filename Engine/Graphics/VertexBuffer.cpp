#include "VertexBuffer.h"

namespace UnoEngine {

void VertexBuffer::Create(ID3D12Device* device, const void* data, uint32 size, uint32 stride) {
    vertexCount_ = size / stride;

    // ヒーププロパティ（アップロードヒープ）
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // リソース記述
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = size;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // バッファ作成
    ThrowIfFailed(
        device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&buffer_)
        ),
        "Failed to create vertex buffer"
    );

    // データコピー
    void* mapped = nullptr;
    ThrowIfFailed(
        buffer_->Map(0, nullptr, &mapped),
        "Failed to map vertex buffer"
    );
    memcpy(mapped, data, size);
    buffer_->Unmap(0, nullptr);

    // ビュー作成
    view_.BufferLocation = buffer_->GetGPUVirtualAddress();
    view_.SizeInBytes = size;
    view_.StrideInBytes = stride;
}

} // namespace UnoEngine
