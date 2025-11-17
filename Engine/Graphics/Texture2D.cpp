#include "Texture2D.h"
#include "GraphicsDevice.h"
#include "d3dx12.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace UnoEngine {

void Texture2D::LoadFromFile(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                             const std::wstring& filepath, uint32 srvIndex) {
    auto* device = graphics->GetDevice();

    DirectX::TexMetadata metadata;
    DirectX::ScratchImage scratchImage;

    ThrowIfFailed(
        DirectX::LoadFromWICFile(filepath.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, scratchImage),
        "Failed to load texture file"
    );

    // 元のメタデータを保存
    DirectX::TexMetadata originalMetadata = metadata;
    
    // リソース作成用にsRGBフォーマットに変換（ガンマ補正用）
    if (!DirectX::IsSRGB(metadata.format)) {
        metadata.format = DirectX::MakeSRGB(metadata.format);
    }

    ThrowIfFailed(
        DirectX::CreateTexture(device, metadata, &resource_),
        "Failed to create texture resource"
    );

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ThrowIfFailed(
        DirectX::PrepareUpload(device, scratchImage.GetImages(), scratchImage.GetImageCount(),
                              originalMetadata, subresources),
        "Failed to prepare texture upload"
    );

    const uint64 uploadBufferSize = GetRequiredIntermediateSize(resource_.Get(), 0,
                                                                static_cast<uint32>(subresources.size()));

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(
        device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer_)
        ),
        "Failed to create upload buffer"
    );

    UpdateSubresources(commandList, resource_.Get(), uploadBuffer_.Get(),
                      0, 0, static_cast<uint32>(subresources.size()), subresources.data());

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);

    width_ = static_cast<uint32>(metadata.width);
    height_ = static_cast<uint32>(metadata.height);
    mipLevels_ = static_cast<uint32>(metadata.mipLevels);
    srvIndex_ = srvIndex;

    graphics->CreateSRV(resource_.Get(), srvIndex);
}

void Texture2D::CreateFromData(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                               const void* data, uint32 width, uint32 height,
                               uint32 srvIndex, bool generateMips) {
    auto* device = graphics->GetDevice();
    width_ = width;
    height_ = height;
    srvIndex_ = srvIndex;
    mipLevels_ = 1;

    const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(
        device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&resource_)
        ),
        "Failed to create texture resource"
    );

    const uint64 uploadBufferSize = GetRequiredIntermediateSize(resource_.Get(), 0, 1);

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    ThrowIfFailed(
        device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer_)
        ),
        "Failed to create upload buffer"
    );

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = data;
    textureData.RowPitch = width * 4;
    textureData.SlicePitch = textureData.RowPitch * height;

    const uint64 uploadedBytes = UpdateSubresources(commandList, resource_.Get(), uploadBuffer_.Get(),
                      0, 0, 1, &textureData);

    if (uploadedBytes == 0) {
        throw std::runtime_error("Failed to upload texture data");
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);

    graphics->CreateSRV(resource_.Get(), srvIndex);
}

} // namespace UnoEngine
