#pragma once

#include "D3D12Common.h"
#include "../Core/NonCopyable.h"
#include "../Core/Types.h"

namespace UnoEngine {

class GraphicsDevice;

class RenderTexture : public NonCopyable {
public:
    RenderTexture() = default;
    ~RenderTexture() = default;

    void Create(GraphicsDevice* graphics, uint32 width, uint32 height, uint32 srvIndex);
    void Resize(GraphicsDevice* graphics, uint32 width, uint32 height);
    void Release();

    ID3D12Resource* GetResource() const { return renderTarget_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return srvHandle_; }

    uint32 GetWidth() const { return width_; }
    uint32 GetHeight() const { return height_; }

private:
    ComPtr<ID3D12Resource> renderTarget_;
    ComPtr<ID3D12Resource> depthStencil_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_ = {};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle_ = {};

    uint32 width_ = 0;
    uint32 height_ = 0;
    uint32 srvIndex_ = 0;
};

} // namespace UnoEngine
