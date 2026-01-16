#include "pch.h"
#include "VideoDecoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace UnoEngine {

VideoDecoder::~VideoDecoder() {
    Close();
}

bool VideoDecoder::Open(const std::string& filepath) {
    Close();

    // ファイルを開く
    if (avformat_open_input(&m_formatCtx, filepath.c_str(), nullptr, nullptr) < 0) {
        return false;
    }

    // ストリーム情報を取得
    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        Close();
        return false;
    }

    // ビデオストリームを探す
    m_videoStreamIndex = -1;
    for (uint32 i = 0; i < m_formatCtx->nb_streams; ++i) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (m_videoStreamIndex < 0) {
        Close();
        return false;
    }

    if (!InitializeDecoder()) {
        Close();
        return false;
    }

    // メタデータ取得
    AVStream* videoStream = m_formatCtx->streams[m_videoStreamIndex];
    m_width = static_cast<uint32>(m_codecCtx->width);
    m_height = static_cast<uint32>(m_codecCtx->height);

    if (videoStream->avg_frame_rate.den != 0) {
        m_frameRate = av_q2d(videoStream->avg_frame_rate);
    } else if (videoStream->r_frame_rate.den != 0) {
        m_frameRate = av_q2d(videoStream->r_frame_rate);
    } else {
        m_frameRate = 30.0;
    }

    if (m_formatCtx->duration != AV_NOPTS_VALUE) {
        m_duration = static_cast<double>(m_formatCtx->duration) / AV_TIME_BASE;
    }

    // RGBAバッファ確保
    m_rgbaBuffer.resize(m_width * m_height * 4);

    // RGBAフレーム用バッファ設定
    av_image_fill_arrays(
        m_rgbaFrame->data,
        m_rgbaFrame->linesize,
        m_rgbaBuffer.data(),
        AV_PIX_FMT_RGBA,
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        1);

    // swscaleコンテキスト作成
    m_swsCtx = sws_getContext(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        m_codecCtx->pix_fmt,
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr, nullptr, nullptr);

    if (!m_swsCtx) {
        Close();
        return false;
    }

    m_eof = false;
    m_currentTime = 0.0;

    return true;
}

bool VideoDecoder::InitializeDecoder() {
    AVStream* videoStream = m_formatCtx->streams[m_videoStreamIndex];

    // デコーダーを探す
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        return false;
    }

    // コーデックコンテキスト作成
    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        return false;
    }

    if (avcodec_parameters_to_context(m_codecCtx, videoStream->codecpar) < 0) {
        return false;
    }

    // デコーダーを開く
    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        return false;
    }

    // フレーム/パケット確保
    m_frame = av_frame_alloc();
    m_rgbaFrame = av_frame_alloc();
    m_packet = av_packet_alloc();

    if (!m_frame || !m_rgbaFrame || !m_packet) {
        return false;
    }

    return true;
}

void VideoDecoder::Close() {
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_packet) {
        av_packet_free(&m_packet);
    }

    if (m_rgbaFrame) {
        av_frame_free(&m_rgbaFrame);
    }

    if (m_frame) {
        av_frame_free(&m_frame);
    }

    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }

    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
    }

    m_rgbaBuffer.clear();
    m_videoStreamIndex = -1;
    m_width = 0;
    m_height = 0;
    m_frameRate = 0.0;
    m_duration = 0.0;
    m_currentTime = 0.0;
    m_eof = false;
}

bool VideoDecoder::DecodeNextFrame() {
    if (!m_formatCtx || !m_codecCtx) {
        return false;
    }

    while (true) {
        int ret = av_read_frame(m_formatCtx, m_packet);

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                if (m_looping) {
                    Seek(0.0);
                    continue;
                }
                m_eof = true;
            }
            return false;
        }

        if (m_packet->stream_index != m_videoStreamIndex) {
            av_packet_unref(m_packet);
            continue;
        }

        // パケットをデコーダーに送信
        ret = avcodec_send_packet(m_codecCtx, m_packet);
        av_packet_unref(m_packet);

        if (ret < 0) {
            continue;
        }

        // フレームを受信
        ret = avcodec_receive_frame(m_codecCtx, m_frame);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        }

        if (ret < 0) {
            return false;
        }

        // タイムスタンプ更新
        AVStream* videoStream = m_formatCtx->streams[m_videoStreamIndex];
        if (m_frame->pts != AV_NOPTS_VALUE) {
            m_currentTime = static_cast<double>(m_frame->pts) * av_q2d(videoStream->time_base);
        }

        // RGBA変換
        ConvertFrameToRGBA();

        return true;
    }
}

void VideoDecoder::ConvertFrameToRGBA() {
    sws_scale(
        m_swsCtx,
        m_frame->data,
        m_frame->linesize,
        0,
        static_cast<int>(m_height),
        m_rgbaFrame->data,
        m_rgbaFrame->linesize);
}

bool VideoDecoder::Seek(double timeInSeconds) {
    if (!m_formatCtx) {
        return false;
    }

    int64_t timestamp = static_cast<int64_t>(timeInSeconds * AV_TIME_BASE);

    if (av_seek_frame(m_formatCtx, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        return false;
    }

    avcodec_flush_buffers(m_codecCtx);
    m_currentTime = timeInSeconds;
    m_eof = false;

    return true;
}

} // namespace UnoEngine
