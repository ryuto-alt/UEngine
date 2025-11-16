#pragma once

#include "D3D12Common.h"
#include "../Core/Types.h"

namespace UnoEngine {

// 頂点バッファ
class VertexBuffer {
public:
    VertexBuffer() = default;
    ~VertexBuffer() = default;

    // 作成
    void Create(ID3D12Device* device, const void* data, uint32 size, uint32 stride);

    // アクセサ
    D3D12_VERTEX_BUFFER_VIEW GetView() const { return view_; }
    uint32 GetVertexCount() const { return vertexCount_; }

private:
    ComPtr<ID3D12Resource> buffer_;
    D3D12_VERTEX_BUFFER_VIEW view_ = {};
    uint32 vertexCount_ = 0;
};

} // namespace UnoEngine
