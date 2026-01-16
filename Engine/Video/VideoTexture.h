#pragma once

#include "../Graphics/D3D12Common.h"
#include "../Core/NonCopyable.h"
#include "../Core/Types.h"
#include <array>
#include <vector>

namespace UnoEngine {

class GraphicsDevice;

// GPU動的テクスチャ（ビデオフレーム用）
// リングバッファでフレーム遅延なく更新可能
class VideoTexture : public NonCopyable {
public:
    VideoTexture() = default;
    ~VideoTexture();

    void Create(GraphicsDevice* graphics, uint32 width, uint32 height, uint32 srvIndex);
    void Release();

    // フレームデータをステージング（CPUバッファにコピーのみ）
    void StageFrame(const void* rgbaData, uint32 rowPitch);

    // ステージングしたフレームをGPUにアップロード（コマンドリストがオープン時に呼ぶ）
    void UploadStagedFrame(ID3D12GraphicsCommandList* commandList);

    // ペンディングフレームがあるか
    bool HasPendingFrame() const { return m_hasPendingFrame; }

    ID3D12Resource* GetResource() const { return m_texture.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return m_srvHandle; }
    uint32 GetSRVIndex() const { return m_srvIndex; }
    uint32 GetWidth() const { return m_width; }
    uint32 GetHeight() const { return m_height; }
    bool IsValid() const { return m_texture != nullptr; }

private:
    ComPtr<ID3D12Resource> m_texture;
    std::array<ComPtr<ID3D12Resource>, BACK_BUFFER_COUNT> m_uploadBuffers;

    D3D12_GPU_DESCRIPTOR_HANDLE m_srvHandle = {};
    uint32 m_srvIndex = 0;
    uint32 m_width = 0;
    uint32 m_height = 0;
    uint32 m_currentBuffer = 0;

    GraphicsDevice* m_graphics = nullptr;

    // ステージング用
    std::vector<uint8> m_stagingBuffer;
    uint32 m_stagingRowPitch = 0;
    bool m_hasPendingFrame = false;
    bool m_isFirstUpload = true;
};

} // namespace UnoEngine
