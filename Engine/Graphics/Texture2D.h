#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "D3D12Common.h"
#include <vector>
#include <string>
#include <DirectXTex.h>

namespace UnoEngine {

class GraphicsDevice;

// テクスチャの用途を指定（BC圧縮フォーマット選択に使用）
enum class TextureType {
    Color,         // カラーテクスチャ → BC7_UNORM (sRGB SRVで読み取り)
    Normal,        // 法線マップ → BC5_UNORM (2チャンネル)
    SingleChannel, // ラフネス/メタリック等 → BC4_UNORM (1チャンネル)
    Auto           // ファイル名から自動判定
};

class Texture2D : public NonCopyable {
public:
    Texture2D() = default;
    ~Texture2D() = default;
    Texture2D(Texture2D&&) = default;
    Texture2D& operator=(Texture2D&&) = default;

    void LoadFromFile(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                     const std::wstring& filepath, uint32 srvIndex,
                     TextureType type = TextureType::Auto);

    void CreateFromData(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                       const void* data, uint32 width, uint32 height,
                       uint32 srvIndex, bool generateMips = true);

    // DDSキャッシュを有効/無効にする（デフォルト: 有効）
    static void SetCacheEnabled(bool enabled) { cacheEnabled_ = enabled; }
    static bool IsCacheEnabled() { return cacheEnabled_; }
    // キャッシュディレクトリを設定
    static void SetCacheDirectory(const std::wstring& dir) { cacheDirectory_ = dir; }
    // BC圧縮を有効/無効にする（デフォルト: 有効）
    static void SetCompressionEnabled(bool enabled) { compressionEnabled_ = enabled; }
    static bool IsCompressionEnabled() { return compressionEnabled_; }

    uint32 GetWidth() const { return width_; }
    uint32 GetHeight() const { return height_; }
    uint32 GetMipLevels() const { return mipLevels_; }
    uint32 GetSRVIndex() const { return srvIndex_; }
    bool HasAlphaPixels() const { return hasAlphaPixels_; }
    DXGI_FORMAT GetFormat() const { return format_; }

    // GPU転送完了後にアップロードバッファを解放（GPUメモリリーク防止）
    void ReleaseUploadBuffer() { uploadBuffer_.Reset(); }

private:
    // DDSキャッシュ関連
    static std::wstring GetDDSCachePath(const std::wstring& sourceFilepath);
    static bool IsCacheValid(const std::wstring& sourceFilepath, const std::wstring& cachePath);
    bool TryLoadFromDDSCache(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                             const std::wstring& cachePath, uint32 srvIndex);
    static void SaveDDSCache(const DirectX::ScratchImage& image, const std::wstring& cachePath, bool hasAlpha);

    // BC圧縮関連
    static TextureType DetectTextureType(const std::wstring& filepath);
    static DXGI_FORMAT GetCompressedFormat(TextureType type);
    static bool CanCompress(uint32 width, uint32 height);
    static bool PadToMultipleOf4(DirectX::ScratchImage& image);
    static bool CompressImage(const DirectX::ScratchImage& source, DXGI_FORMAT format,
                              DirectX::ScratchImage& compressed);

    static bool cacheEnabled_;
    static bool compressionEnabled_;
    static std::wstring cacheDirectory_;

    ComPtr<ID3D12Resource> resource_;
    ComPtr<ID3D12Resource> uploadBuffer_;
    uint32 width_ = 0;
    uint32 height_ = 0;
    uint32 mipLevels_ = 0;
    uint32 srvIndex_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    bool hasAlphaPixels_ = false;
};

} // namespace UnoEngine
