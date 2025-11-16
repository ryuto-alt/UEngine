#pragma once

#include "D3D12Common.h"
#include "../Core/Types.h"

namespace UnoEngine {

// 定数バッファ
template<typename T>
class ConstantBuffer {
public:
    ConstantBuffer() = default;
    ~ConstantBuffer() = default;

    // 作成
    void Create(ID3D12Device* device) {
        const uint32 size = (sizeof(T) + 255) & ~255; // 256バイトアライメント

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = size;
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(
            device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&buffer_)
            ),
            "Failed to create constant buffer"
        );

        // マップ（常時マップ状態を維持）
        ThrowIfFailed(
            buffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_)),
            "Failed to map constant buffer"
        );

        gpuAddress_ = buffer_->GetGPUVirtualAddress();
    }

    // データ更新
    void Update(const T& data) {
        if (mappedData_) {
            memcpy(mappedData_, &data, sizeof(T));
        }
    }

    // アクセサ
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return gpuAddress_; }
    ID3D12Resource* GetResource() const { return buffer_.Get(); }

private:
    ComPtr<ID3D12Resource> buffer_;
    T* mappedData_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress_ = 0;
};

} // namespace UnoEngine
