#pragma once

#include "../Core/NonCopyable.h"
#include "../Core/Types.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

// FFmpeg forward declarations
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace UnoEngine {

// FFmpegデコーダーラッパー
// ビデオファイルをデコードしてRGBAフレームを出力
class VideoDecoder : public NonCopyable {
public:
    VideoDecoder() = default;
    ~VideoDecoder();

    bool Open(const std::string& filepath);
    void Close();

    // 次のフレームをデコード（RGBAデータを内部バッファに格納）
    // 戻り値: true=成功, false=EOF or エラー
    bool DecodeNextFrame();

    // 現在のフレームデータを取得
    const uint8* GetFrameData() const { return m_rgbaBuffer.data(); }
    uint32 GetWidth() const { return m_width; }
    uint32 GetHeight() const { return m_height; }
    uint32 GetRowPitch() const { return m_width * 4; }
    double GetFrameRate() const { return m_frameRate; }
    double GetDuration() const { return m_duration; }
    double GetCurrentTime() const { return m_currentTime; }
    bool IsOpen() const { return m_formatCtx != nullptr; }
    bool IsEndOfFile() const { return m_eof; }

    // シーク（秒単位）
    bool Seek(double timeInSeconds);

    // ループ再生設定
    void SetLooping(bool loop) { m_looping = loop; }
    bool IsLooping() const { return m_looping; }

private:
    bool InitializeDecoder();
    void ConvertFrameToRGBA();

private:
    AVFormatContext* m_formatCtx = nullptr;
    AVCodecContext* m_codecCtx = nullptr;
    AVFrame* m_frame = nullptr;
    AVFrame* m_rgbaFrame = nullptr;
    AVPacket* m_packet = nullptr;
    SwsContext* m_swsCtx = nullptr;

    std::vector<uint8> m_rgbaBuffer;

    int m_videoStreamIndex = -1;
    uint32 m_width = 0;
    uint32 m_height = 0;
    double m_frameRate = 0.0;
    double m_duration = 0.0;
    double m_currentTime = 0.0;

    std::atomic<bool> m_eof = false;
    bool m_looping = true;
};

} // namespace UnoEngine
