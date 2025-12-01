#pragma once

#include "../Core/Component.h"
#include <xaudio2.h>
#include <x3daudio.h>
#include <vector>
#include <string>
#include <memory>

namespace UnoEngine {

class AudioClip;
class AudioSystem;

// 音源コンポーネント
class AudioSource : public Component {
public:
    AudioSource() = default;
    ~AudioSource() override;

    // Component lifecycle
    void Awake() override;
    void Start() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // 再生制御
    void Play();
    void Stop();
    void Pause();
    void Resume();
    bool IsPlaying() const;

    // AudioClip設定
    void SetClip(std::shared_ptr<AudioClip> clip);
    std::shared_ptr<AudioClip> GetClip() const { return clip_; }
    
    // ファイルパスから直接ロード
    bool LoadClip(const std::string& filePath);
    const std::string& GetClipPath() const { return clipPath_; }
    void SetClipPath(const std::string& path) { clipPath_ = path; }

    // プロパティ
    float GetVolume() const { return volume_; }
    void SetVolume(float volume);

    bool IsLooping() const { return loop_; }
    void SetLoop(bool loop) { loop_ = loop; }

    bool GetPlayOnAwake() const { return playOnAwake_; }
    void SetPlayOnAwake(bool playOnAwake) { playOnAwake_ = playOnAwake; }

    // 3Dオーディオ設定
    bool Is3D() const { return is3D_; }
    void Set3D(bool is3D) { is3D_ = is3D; }

    float GetMinDistance() const { return minDistance_; }
    void SetMinDistance(float distance) { minDistance_ = distance; }

    float GetMaxDistance() const { return maxDistance_; }
    void SetMaxDistance(float distance) { maxDistance_ = distance; }

private:
    void Update3DAudio();
    void InitializeDSPSettings();
    void CleanupDSPSettings();
    void SubmitBuffer();

    std::shared_ptr<AudioClip> clip_;
    std::string clipPath_;
    IXAudio2SourceVoice* voice_ = nullptr;
    AudioSystem* audioSystem_ = nullptr;

    float volume_ = 1.0f;
    bool loop_ = false;
    bool playOnAwake_ = false;
    bool is3D_ = false;
    float minDistance_ = 1.0f;
    float maxDistance_ = 100.0f;

    bool isPlaying_ = false;
    bool isPaused_ = false;

    // X3DAudio用
    X3DAUDIO_EMITTER emitter_{};
    X3DAUDIO_DSP_SETTINGS dspSettings_{};
    std::vector<float> matrixCoefficients_;
    bool dspInitialized_ = false;
};

} // namespace UnoEngine
