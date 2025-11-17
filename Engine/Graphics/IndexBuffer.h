#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "D3D12Common.h"

namespace UnoEngine {

class IndexBuffer : public NonCopyable {
public:
    IndexBuffer() = default;
    ~IndexBuffer() = default;
    IndexBuffer(IndexBuffer&&) = default;
    IndexBuffer& operator=(IndexBuffer&&) = default;

    void Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                const uint32* indices, uint32 indexCount);

    D3D12_INDEX_BUFFER_VIEW GetView() const { return view_; }
    uint32 GetIndexCount() const { return indexCount_; }

private:
    ComPtr<ID3D12Resource> buffer_;
    ComPtr<ID3D12Resource> uploadBuffer_;  // GPU処理完了まで保持
    D3D12_INDEX_BUFFER_VIEW view_ = {};
    uint32 indexCount_ = 0;
};

} // namespace UnoEngine
