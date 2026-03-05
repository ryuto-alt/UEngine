#include "pch.h"
#include "Texture2D.h"
#include "GraphicsDevice.h"
#include "d3dx12.h"
#include "../Core/Logger.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace UnoEngine {

void Texture2D::LoadFromFile(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                             const std::wstring& filepath, uint32 srvIndex) {
    auto* device = graphics->GetDevice();
    Logger::Debug("[Texture2D] LoadFromFile 開始: SRV={}", srvIndex);

    DirectX::TexMetadata metadata;
    DirectX::ScratchImage scratchImage;

    Logger::Debug("[Texture2D] WICファイル読み込み中...");
    ThrowIfFailed(
        DirectX::LoadFromWICFile(filepath.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, scratchImage),
        "Failed to load texture file"
    );
    Logger::Debug("[Texture2D] WIC読み込み完了: {}x{}, format={}", metadata.width, metadata.height, static_cast<int>(metadata.format));

    // フォーマットがR8G8B8A8_UNORMでない場合は変換（BGRAやRGB等の不一致を防ぐ）
    if (metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        DirectX::ScratchImage converted;
        HRESULT hr = DirectX::Convert(
            scratchImage.GetImages(), scratchImage.GetImageCount(),
            scratchImage.GetMetadata(), DXGI_FORMAT_R8G8B8A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT,
            converted
        );
        if (SUCCEEDED(hr)) {
            scratchImage = std::move(converted);
            metadata = scratchImage.GetMetadata();
        }
    }

    // 元のメタデータを保存
    DirectX::TexMetadata originalMetadata = metadata;
    
    // Calculate mip levels for GPU generation
    uint32 maxDim = static_cast<uint32>(std::max(metadata.width, metadata.height));
    uint32 mipLevels = static_cast<uint32>(std::floor(std::log2(maxDim))) + 1;
    
    // UAVはsRGBフォーマットをサポートしないため、非sRGBフォーマットを使用
    // シェーダー内でsRGB変換を行う
    DXGI_FORMAT resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Create texture with full mip chain and UAV flag for GPU mip generation
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = metadata.width;
    texDesc.Height = static_cast<UINT>(metadata.height);
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = static_cast<UINT16>(mipLevels);
    texDesc.Format = resourceFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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

    // Only upload the base mip level
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ThrowIfFailed(
        DirectX::PrepareUpload(device, scratchImage.GetImages(), 1,
                              originalMetadata, subresources),
        "Failed to prepare texture upload"
    );

    const uint64 uploadBufferSize = GetRequiredIntermediateSize(resource_.Get(), 0, 1);

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

    Logger::Debug("[Texture2D] UpdateSubresources 実行中...");
    UpdateSubresources(commandList, resource_.Get(), uploadBuffer_.Get(),
                      0, 0, 1, subresources.data());
    Logger::Debug("[Texture2D] UpdateSubresources 完了");

    // Generate mipmaps on GPU
    if (mipLevels > 1) {
        Logger::Debug("[Texture2D] MipMap生成開始 (mipLevels={})", mipLevels);
        graphics->GetMipmapGenerator()->GenerateMips(
            commandList, resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST
        );
        Logger::Debug("[Texture2D] MipMap生成完了");
    } else {
        // No mips to generate, just transition to shader resource
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        commandList->ResourceBarrier(1, &barrier);
    }

    width_ = static_cast<uint32>(metadata.width);
    height_ = static_cast<uint32>(metadata.height);
    mipLevels_ = mipLevels;
    srvIndex_ = srvIndex;

    // Check if the source image has any non-opaque pixels (for alpha clip detection)
    hasAlphaPixels_ = false;
    const auto* img = scratchImage.GetImage(0, 0, 0);
    if (img && img->pixels) {
        const size_t pixelCount = img->width * img->height;
        const uint8_t* pixels = img->pixels;
        const size_t rowPitch = img->rowPitch;
        for (size_t y = 0; y < img->height && !hasAlphaPixels_; ++y) {
            const uint8_t* row = pixels + y * rowPitch;
            for (size_t x = 0; x < img->width; ++x) {
                if (row[x * 4 + 3] < 250) {  // Alpha channel < ~0.98
                    hasAlphaPixels_ = true;
                    break;
                }
            }
        }
    }

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
