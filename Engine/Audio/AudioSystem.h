#pragma once

#include "../Systems/ISystem.h"
#include <xaudio2.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <array>

namespace UnoEngine {

class AudioClip;
class AudioSource;

// XAudio2を管理するシステム
class AudioSystem : public ISystem {
public:
    static constexpr int MAX_VOICES = 32;

    AudioSystem();
    ~AudioSystem();

    // シングルトンアクセス
    static AudioSystem* GetInstance() { return instance_; }

    bool Initialize();
    void Shutdown();

    // ISystem
    void OnSceneStart(Scene* scene) override;
    void OnUpdate(Scene* scene, float deltaTime) override;
    void OnSceneEnd(Scene* scene) override;
    int GetPriority() const override { return 10; }

    // ボイス管理
    IXAudio2SourceVoice* AcquireVoice(const WAVEFORMATEX* format);
    void ReleaseVoice(IXAudio2SourceVoice* voice);

    // アクセサ
    IXAudio2* GetXAudio2() const { return xaudio2_.Get(); }
    IXAudio2MasteringVoice* GetMasterVoice() const { return masterVoice_; }
    bool IsInitialized() const { return initialized_; }

    // 一時停止制御
    void PauseAll();
    void ResumeAll();
    bool IsPaused() const { return isPaused_; }

private:
    static AudioSystem* instance_;

    Microsoft::WRL::ComPtr<IXAudio2> xaudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
    bool initialized_ = false;
    bool isPaused_ = false;

    // アクティブなボイスのリスト
    std::vector<IXAudio2SourceVoice*> activeVoices_;
};

} // namespace UnoEngine
