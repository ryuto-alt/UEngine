#include "pch.h"
#include "Texture2D.h"
#include "GraphicsDevice.h"
#include "d3dx12.h"
#include "../Core/Logger.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <functional>

namespace UnoEngine {

// Static members
bool Texture2D::cacheEnabled_ = true;
bool Texture2D::compressionEnabled_ = false;
std::wstring Texture2D::cacheDirectory_ = L".cache/textures";

// ===== BC圧縮ヘルパー =====

// ファイル名からテクスチャの種類を自動判定
TextureType Texture2D::DetectTextureType(const std::wstring& filepath) {
    namespace fs = std::filesystem;
    std::wstring stem = fs::path(filepath).stem().wstring();

    // 小文字化して判定
    std::wstring lower = stem;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    // 法線マップの判定
    if (lower.find(L"normal") != std::wstring::npos ||
        lower.find(L"_n") != std::wstring::npos ||
        lower.find(L"_nm") != std::wstring::npos ||
        lower.find(L"normaldx") != std::wstring::npos ||
        lower.find(L"normalgl") != std::wstring::npos) {
        return TextureType::Normal;
    }

    // 単チャンネル（ラフネス、メタリック、AO、ディスプレースメント、オパシティ）の判定
    if (lower.find(L"roughness") != std::wstring::npos ||
        lower.find(L"metallic") != std::wstring::npos ||
        lower.find(L"_ao") != std::wstring::npos ||
        lower.find(L"displacement") != std::wstring::npos ||
        lower.find(L"opacity") != std::wstring::npos) {
        return TextureType::SingleChannel;
    }

    // メタリックラフネス複合テクスチャはBC7で圧縮（複数チャンネル使用）
    if (lower.find(L"metallicroughness") != std::wstring::npos ||
        lower.find(L"_ors") != std::wstring::npos) {
        return TextureType::Color;
    }

    return TextureType::Color;
}

// テクスチャの種類に応じたBC圧縮フォーマットを返す
DXGI_FORMAT Texture2D::GetCompressedFormat(TextureType type) {
    switch (type) {
    case TextureType::Normal:
        return DXGI_FORMAT_BC5_UNORM;
    case TextureType::SingleChannel:
        return DXGI_FORMAT_BC4_UNORM;
    case TextureType::Color:
    default:
        return DXGI_FORMAT_BC7_UNORM;
    }
}

// BC圧縮可能かチェック（小さすぎるテクスチャは圧縮しない）
bool Texture2D::CanCompress(uint32 width, uint32 height) {
    // BC圧縮は4x4ブロック単位なので最低4x4必要
    // 効果が薄い小さなテクスチャはスキップ
    return width >= 64 && height >= 64;
}

// 幅・高さを4の倍数にパディング（BC圧縮の必須要件）
bool Texture2D::PadToMultipleOf4(DirectX::ScratchImage& image) {
    const auto& meta = image.GetMetadata();
    size_t newWidth = (meta.width + 3) & ~3;
    size_t newHeight = (meta.height + 3) & ~3;

    if (newWidth == meta.width && newHeight == meta.height) {
        return true; // パディング不要
    }

    DirectX::ScratchImage resized;
    HRESULT hr = DirectX::Resize(
        image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        newWidth, newHeight, DirectX::TEX_FILTER_DEFAULT,
        resized
    );

    if (FAILED(hr)) {
        Logger::Warning("[Texture2D] 4の倍数へのリサイズ失敗: {}x{} -> {}x{}",
                        meta.width, meta.height, newWidth, newHeight);
        return false;
    }

    image = std::move(resized);
    return true;
}

// DirectXTex BCn圧縮を実行
bool Texture2D::CompressImage(const DirectX::ScratchImage& source, DXGI_FORMAT format,
                               DirectX::ScratchImage& compressed) {
    DirectX::TEX_COMPRESS_FLAGS compressFlags = DirectX::TEX_COMPRESS_PARALLEL;

    // BC7はBC7_QUICKフラグで高速圧縮
    if (format == DXGI_FORMAT_BC7_UNORM || format == DXGI_FORMAT_BC7_UNORM_SRGB) {
        compressFlags |= DirectX::TEX_COMPRESS_BC7_QUICK;
    }

    HRESULT hr = DirectX::Compress(
        source.GetImages(), source.GetImageCount(), source.GetMetadata(),
        format, compressFlags, 1.0f, compressed
    );

    if (FAILED(hr)) {
        Logger::Warning("[Texture2D] BC圧縮失敗: format={}, hr=0x{:08X}",
                        static_cast<int>(format), static_cast<unsigned>(hr));
        return false;
    }

    return true;
}

// ===== DDSキャッシュ =====

// ソースファイルパスからDDSキャッシュパスを生成
std::wstring Texture2D::GetDDSCachePath(const std::wstring& sourceFilepath) {
    namespace fs = std::filesystem;

    // ソースパスのハッシュをファイル名として使用
    size_t hash = std::hash<std::wstring>{}(fs::absolute(sourceFilepath).wstring());
    std::wstring hashStr = std::to_wstring(hash);

    // 元のファイル名も含めてデバッグしやすくする
    // キャッシュバージョン: BC圧縮導入時にv2に変更（旧キャッシュ自動無効化）
    std::wstring stem = fs::path(sourceFilepath).stem().wstring();
    return cacheDirectory_ + L"/" + stem + L"_" + hashStr + L"_v2.dds";
}

// キャッシュが有効かチェック（ソースファイルより新しいか）
bool Texture2D::IsCacheValid(const std::wstring& sourceFilepath, const std::wstring& cachePath) {
    namespace fs = std::filesystem;

    if (!fs::exists(cachePath)) {
        return false;
    }

    // キャッシュがソースより古ければ無効
    auto sourceTime = fs::last_write_time(sourceFilepath);
    auto cacheTime = fs::last_write_time(cachePath);
    return cacheTime >= sourceTime;
}

// DDSキャッシュからテクスチャを読み込む
bool Texture2D::TryLoadFromDDSCache(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                     const std::wstring& cachePath, uint32 srvIndex) {
    auto* device = graphics->GetDevice();

    DirectX::TexMetadata metadata;
    DirectX::ScratchImage scratchImage;

    HRESULT hr = DirectX::LoadFromDDSFile(cachePath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage);
    if (FAILED(hr)) {
        return false;
    }

    // DDSにはミップマップが全て含まれている
    uint32 mipLevels = static_cast<uint32>(metadata.mipLevels);

    // テクスチャリソース作成
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = metadata.width;
    texDesc.Height = static_cast<UINT>(metadata.height);
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = static_cast<UINT16>(mipLevels);
    texDesc.Format = metadata.format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // UAV不要（ミップ生成しない）

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
        "Failed to create cached texture resource"
    );

    // 全ミップレベルをアップロード
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ThrowIfFailed(
        DirectX::PrepareUpload(device, scratchImage.GetImages(), scratchImage.GetImageCount(),
                              metadata, subresources),
        "Failed to prepare cached texture upload"
    );

    const uint64 uploadBufferSize = GetRequiredIntermediateSize(resource_.Get(), 0, static_cast<UINT>(subresources.size()));

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
        "Failed to create cached upload buffer"
    );

    UpdateSubresources(commandList, resource_.Get(), uploadBuffer_.Get(),
                      0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    // COPY_DEST -> PIXEL_SHADER_RESOURCE に遷移
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    commandList->ResourceBarrier(1, &barrier);

    width_ = static_cast<uint32>(metadata.width);
    height_ = static_cast<uint32>(metadata.height);
    mipLevels_ = mipLevels;
    srvIndex_ = srvIndex;
    format_ = metadata.format;

    // hasAlphaPixelsはキャッシュメタデータファイルから読む
    namespace fs = std::filesystem;
    std::wstring metaPath = cachePath + L".meta";
    hasAlphaPixels_ = fs::exists(metaPath);

    graphics->CreateSRV(resource_.Get(), srvIndex);
    return true;
}

// ミップマップ付きScratchImageをDDSとしてキャッシュ保存
void Texture2D::SaveDDSCache(const DirectX::ScratchImage& image, const std::wstring& cachePath, bool hasAlpha) {
    namespace fs = std::filesystem;

    // キャッシュディレクトリを作成
    fs::path cacheDir = fs::path(cachePath).parent_path();
    if (!fs::exists(cacheDir)) {
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
        if (ec) {
            Logger::Warning("[Texture2D] キャッシュディレクトリ作成失敗: {}", ec.message());
            return;
        }
    }

    // DDS保存
    HRESULT hr = DirectX::SaveToDDSFile(
        image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DirectX::DDS_FLAGS_NONE, cachePath.c_str()
    );

    if (FAILED(hr)) {
        Logger::Warning("[Texture2D] DDSキャッシュ保存失敗");
        return;
    }

    // hasAlphaPixelsをメタファイルとして保存
    if (hasAlpha) {
        std::wstring metaPath = cachePath + L".meta";
        // 空ファイルを作成（存在するだけでalpha=trueを示す）
        std::ofstream metaFile(metaPath);
    }

    Logger::Debug("[Texture2D] DDSキャッシュ保存完了");
}

// ===== メインロード関数 =====

void Texture2D::LoadFromFile(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                             const std::wstring& filepath, uint32 srvIndex,
                             TextureType type) {
    auto* device = graphics->GetDevice();
    Logger::Debug("[Texture2D] LoadFromFile 開始: SRV={}", srvIndex);

    // DDSキャッシュチェック（BC圧縮済みデータがキャッシュされている）
    if (cacheEnabled_) {
        std::wstring cachePath = GetDDSCachePath(filepath);
        if (IsCacheValid(filepath, cachePath)) {
            if (TryLoadFromDDSCache(graphics, commandList, cachePath, srvIndex)) {
                Logger::Debug("[Texture2D] DDSキャッシュからロード完了: SRV={}", srvIndex);
                return;
            }
        }
    }

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

    // Check if the source image has any non-opaque pixels (for alpha clip detection)
    hasAlphaPixels_ = false;
    const auto* img = scratchImage.GetImage(0, 0, 0);
    if (img && img->pixels) {
        const uint8_t* pixels = img->pixels;
        const size_t rowPitch = img->rowPitch;
        for (size_t y = 0; y < img->height && !hasAlphaPixels_; ++y) {
            const uint8_t* row = pixels + y * rowPitch;
            for (size_t x = 0; x < img->width; ++x) {
                if (row[x * 4 + 3] < 250) {
                    hasAlphaPixels_ = true;
                    break;
                }
            }
        }
    }

    // テクスチャタイプの自動判定
    if (type == TextureType::Auto) {
        type = DetectTextureType(filepath);
    }

    // BC圧縮の適用判定
    bool useCompression = compressionEnabled_ &&
                          CanCompress(static_cast<uint32>(metadata.width), static_cast<uint32>(metadata.height));
    DXGI_FORMAT compressedFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (useCompression) {
        compressedFormat = GetCompressedFormat(type);
        Logger::Debug("[Texture2D] BC圧縮予定: type={}, format={}",
                      static_cast<int>(type), static_cast<int>(compressedFormat));
    }

    // 4の倍数パディング（BC圧縮する場合のみ）
    if (useCompression) {
        if (!PadToMultipleOf4(scratchImage)) {
            Logger::Warning("[Texture2D] 4の倍数パディング失敗、非圧縮にフォールバック");
            useCompression = false;
        } else {
            metadata = scratchImage.GetMetadata();
        }
    }

    // CPUでミップマップを生成（非圧縮データから生成、その後圧縮）
    DirectX::ScratchImage mipChain;
    uint32 mipLevels;
    bool hasCPUMips = false;

    if (metadata.width > 1 || metadata.height > 1) {
        HRESULT hr = DirectX::GenerateMipMaps(
            scratchImage.GetImages(), scratchImage.GetImageCount(),
            scratchImage.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT, 0, // 0 = 全レベル生成
            mipChain
        );
        if (SUCCEEDED(hr)) {
            hasCPUMips = true;
            metadata = mipChain.GetMetadata();
            mipLevels = static_cast<uint32>(metadata.mipLevels);
        } else {
            // CPU mip生成失敗時はフォールバック
            mipLevels = static_cast<uint32>(std::floor(std::log2(
                static_cast<float>(std::max(metadata.width, metadata.height))))) + 1;
        }
    } else {
        mipLevels = 1;
    }

    // BC圧縮実行（ミップチェーン全体を圧縮）
    DirectX::ScratchImage compressedImage;
    bool isCompressed = false;

    if (useCompression && hasCPUMips) {
        if (CompressImage(mipChain, compressedFormat, compressedImage)) {
            isCompressed = true;
            Logger::Debug("[Texture2D] BC圧縮完了: {}x{} format={}",
                          compressedImage.GetMetadata().width,
                          compressedImage.GetMetadata().height,
                          static_cast<int>(compressedFormat));
        } else {
            Logger::Warning("[Texture2D] BC圧縮失敗、非圧縮にフォールバック");
        }
    } else if (useCompression && !hasCPUMips) {
        // ミップなしで圧縮を試行
        if (CompressImage(scratchImage, compressedFormat, compressedImage)) {
            isCompressed = true;
        }
    }

    // アップロードに使用するイメージを決定
    const DirectX::ScratchImage* uploadImagePtr = nullptr;
    if (isCompressed) {
        uploadImagePtr = &compressedImage;
    } else if (hasCPUMips) {
        uploadImagePtr = &mipChain;
    } else {
        uploadImagePtr = &scratchImage;
    }

    const auto& uploadImage = *uploadImagePtr;
    const auto& uploadMetadata = uploadImage.GetMetadata();

    // DDSキャッシュに保存（圧縮済みデータ or ミップマップ付き非圧縮データ）
    if (cacheEnabled_ && (isCompressed || hasCPUMips)) {
        std::wstring cachePath = GetDDSCachePath(filepath);
        SaveDDSCache(uploadImage, cachePath, hasAlphaPixels_);
    }

    // GPUリソースの作成とアップロード
    DXGI_FORMAT resourceFormat = isCompressed ? compressedFormat : DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = uploadMetadata.width;
    texDesc.Height = static_cast<UINT>(uploadMetadata.height);
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = static_cast<UINT16>(isCompressed || hasCPUMips ? mipLevels : mipLevels);
    texDesc.Format = resourceFormat;
    texDesc.SampleDesc.Count = 1;
    // BC圧縮済みまたはCPUミップがある場合はUAV不要
    texDesc.Flags = (isCompressed || hasCPUMips) ? D3D12_RESOURCE_FLAG_NONE : D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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

    if (isCompressed || hasCPUMips) {
        // 全ミップレベルをアップロード（GPU mip生成不要）
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
        ThrowIfFailed(
            DirectX::PrepareUpload(device, uploadImage.GetImages(), uploadImage.GetImageCount(),
                                  uploadMetadata, subresources),
            "Failed to prepare texture upload"
        );

        const uint64 uploadBufferSize = GetRequiredIntermediateSize(resource_.Get(), 0, static_cast<UINT>(subresources.size()));

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
                          0, 0, static_cast<UINT>(subresources.size()), subresources.data());

        // COPY_DEST -> PIXEL_SHADER_RESOURCE
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        commandList->ResourceBarrier(1, &barrier);
    } else {
        // フォールバック: 従来のGPU mip生成パス
        DirectX::TexMetadata originalMetadata = scratchImage.GetMetadata();
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

        UpdateSubresources(commandList, resource_.Get(), uploadBuffer_.Get(),
                          0, 0, 1, subresources.data());

        if (mipLevels > 1) {
            graphics->GetMipmapGenerator()->GenerateMips(
                commandList, resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST
            );
        } else {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                resource_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        }
    }

    width_ = static_cast<uint32>(uploadMetadata.width);
    height_ = static_cast<uint32>(uploadMetadata.height);
    mipLevels_ = mipLevels;
    srvIndex_ = srvIndex;
    format_ = resourceFormat;

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
    format_ = DXGI_FORMAT_R8G8B8A8_UNORM;

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
