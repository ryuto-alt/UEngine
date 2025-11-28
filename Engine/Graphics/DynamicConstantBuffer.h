#pragma once

#include "D3D12Common.h"
#include "../Core/Types.h"
#include <vector>

namespace UnoEngine {

// フレーム内で複数回更新可能なダイナミック定数バッファ
// 各Updateで新しいオフセットにデータを書き込み、そのアドレスを返す
template<typename T>
class DynamicConstantBuffer {
public:
    DynamicConstantBuffer() = default;
    ~DynamicConstantBuffer() = default;

    // 作成（maxUpdatesPerFrame: 1フレーム内の最大更新回数）
    void Create(ID3D12Device* device, uint32 maxUpdatesPerFrame = 256) {
        maxUpdates_ = maxUpdatesPerFrame;
        alignedSize_ = (sizeof(T) + 255) & ~255; // 256バイトアライメント
        totalSize_ = alignedSize_ * maxUpdates_;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = totalSize_;
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
            "Failed to create dynamic constant buffer"
        );

        // 常時マップ状態を維持
        ThrowIfFailed(
            buffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_)),
            "Failed to map dynamic constant buffer"
        );

        baseGpuAddress_ = buffer_->GetGPUVirtualAddress();
    }

    // フレーム開始時にリセット
    void Reset() {
        currentOffset_ = 0;
    }

    // データ更新（新しいGPUアドレスを返す）
    D3D12_GPU_VIRTUAL_ADDRESS Update(const T& data) {
        if (currentOffset_ >= maxUpdates_) {
            // オーバーフロー防止（最初に戻る - 本来はエラー）
            currentOffset_ = 0;
        }

        uint8* dest = mappedData_ + (currentOffset_ * alignedSize_);
        memcpy(dest, &data, sizeof(T));

        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = baseGpuAddress_ + (currentOffset_ * alignedSize_);
        currentOffset_++;

        return gpuAddr;
    }

    ID3D12Resource* GetResource() const { return buffer_.Get(); }

private:
    ComPtr<ID3D12Resource> buffer_;
    uint8* mappedData_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS baseGpuAddress_ = 0;
    
    uint32 maxUpdates_ = 0;
    uint32 alignedSize_ = 0;
    uint32 totalSize_ = 0;
    uint32 currentOffset_ = 0;
};

} // namespace UnoEngine
