#pragma once

#include "../Core/Component.h"
#include "../Core/Types.h"
#include "VideoDecoder.h"
#include "VideoTexture.h"
#include <string>
#include <memory>

namespace UnoEngine {

class GraphicsDevice;
class Material;

// ビデオ再生コンポーネント
// GameObjectにアタッチしてビデオをマテリアルに表示
class VideoPlayerComponent : public Component {
public:
    VideoPlayerComponent() = default;
    ~VideoPlayerComponent() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // ビデオファイルを開く
    bool LoadVideo(const std::string& filepath);

    // 再生コントロール
    void Play();
    void Pause();
    void Stop();
    void SetLooping(bool loop);
    void Seek(double timeInSeconds);

    // 状態取得
    bool IsPlaying() const { return m_isPlaying; }
    bool IsPaused() const { return m_isPaused; }
    bool IsLooping() const { return m_decoder ? m_decoder->IsLooping() : false; }
    double GetCurrentTime() const { return m_decoder ? m_decoder->GetCurrentTime() : 0.0; }
    double GetDuration() const { return m_decoder ? m_decoder->GetDuration() : 0.0; }
    double GetFrameRate() const { return m_decoder ? m_decoder->GetFrameRate() : 0.0; }
    uint32 GetWidth() const { return m_decoder ? m_decoder->GetWidth() : 0; }
    uint32 GetHeight() const { return m_decoder ? m_decoder->GetHeight() : 0; }

    // ターゲットマテリアル設定
    void SetTargetMaterial(Material* material) { m_targetMaterial = material; }
    Material* GetTargetMaterial() const { return m_targetMaterial; }

    // マテリアル名で設定（MeshRenderer内のマテリアルを自動検索）
    void SetTargetMaterialByName(const std::string& materialName);

    // ビデオテクスチャのSRVインデックス取得
    uint32 GetVideoTextureSRVIndex() const;

    // GraphicsDevice設定（シーンから自動設定される）
    void SetGraphicsDevice(GraphicsDevice* graphics) { m_graphics = graphics; }

    // シリアライズ用
    const std::string& GetVideoPath() const { return m_videoPath; }
    void SetVideoPath(const std::string& path) { m_videoPath = path; }
    const std::string& GetTargetMaterialName() const { return m_targetMaterialName; }

    // レンダリング時にGPUアップロード（コマンドリストがオープン時に呼ぶ）
    void UploadVideoFrame(ID3D12GraphicsCommandList* commandList);
    bool HasPendingFrame() const;

private:
    void StageVideoFrame();

private:
    std::unique_ptr<VideoDecoder> m_decoder;
    std::unique_ptr<VideoTexture> m_videoTexture;

    GraphicsDevice* m_graphics = nullptr;
    Material* m_targetMaterial = nullptr;

    std::string m_videoPath;
    std::string m_targetMaterialName;

    double m_frameTimer = 0.0;
    double m_frameInterval = 0.0;

    bool m_isPlaying = false;
    bool m_isPaused = false;
    bool m_needsTextureInit = false;
};

} // namespace UnoEngine
