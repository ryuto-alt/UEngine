#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "D3D12Common.h"
#include <vector>
#include <string>

namespace UnoEngine {

class GraphicsDevice;

class Texture2D : public NonCopyable {
public:
    Texture2D() = default;
    ~Texture2D() = default;
    Texture2D(Texture2D&&) = default;
    Texture2D& operator=(Texture2D&&) = default;

    void LoadFromFile(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                     const std::wstring& filepath, uint32 srvIndex);

    void CreateFromData(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                       const void* data, uint32 width, uint32 height,
                       uint32 srvIndex, bool generateMips = true);

    uint32 GetWidth() const { return width_; }
    uint32 GetHeight() const { return height_; }
    uint32 GetMipLevels() const { return mipLevels_; }
    uint32 GetSRVIndex() const { return srvIndex_; }

private:
    ComPtr<ID3D12Resource> resource_;
    ComPtr<ID3D12Resource> uploadBuffer_;
    uint32 width_ = 0;
    uint32 height_ = 0;
    uint32 mipLevels_ = 0;
    uint32 srvIndex_ = 0;
};

} // namespace UnoEngine
