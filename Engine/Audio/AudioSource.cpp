#include "AudioSource.h"
#include "AudioClip.h"
#include "AudioSystem.h"
#include "AudioListener.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"
#include "../Core/Logger.h"
#include <cmath>

namespace UnoEngine {

AudioSource::~AudioSource() {
    OnDestroy();
}

void AudioSource::Awake() {
    audioSystem_ = AudioSystem::GetInstance();
}

void AudioSource::Start() {
    // クリップパスが設定されていてクリップがない場合はロード
    if (!clip_ && !clipPath_.empty()) {
        LoadClip(clipPath_);
    }

    if (playOnAwake_ && clip_) {
        Play();
    }
}

void AudioSource::OnUpdate(float deltaTime) {
    if (!isPlaying_ || isPaused_) return;

    // 3Dオーディオの音量更新
    if (is3D_) {
        UpdateVolume();
    }

    // 再生終了チェック
    if (voice_) {
        XAUDIO2_VOICE_STATE state;
        voice_->GetState(&state);
        if (state.BuffersQueued == 0 && !loop_) {
            isPlaying_ = false;
        }
    }
}

void AudioSource::OnDestroy() {
    Stop();
    if (voice_ && audioSystem_) {
        audioSystem_->ReleaseVoice(voice_);
        voice_ = nullptr;
    }
}

void AudioSource::Play() {
    if (!clip_ || !clip_->IsLoaded()) {
        Logger::Warning("AudioSource: No clip loaded");
        return;
    }

    // AudioSystemがなければ再取得
    if (!audioSystem_) {
        audioSystem_ = AudioSystem::GetInstance();
    }

    if (!audioSystem_ || !audioSystem_->IsInitialized()) {
        Logger::Warning("AudioSource: AudioSystem not initialized");
        return;
    }

    // 既存のボイスがあれば停止
    if (voice_) {
        voice_->Stop();
        voice_->FlushSourceBuffers();
    } else {
        voice_ = audioSystem_->AcquireVoice(&clip_->GetFormat());
        if (!voice_) {
            Logger::Error("AudioSource: Failed to acquire voice");
            return;
        }
    }

    SubmitBuffer();
    UpdateVolume();
    voice_->Start();
    isPlaying_ = true;
    isPaused_ = false;
}

void AudioSource::Stop() {
    if (voice_) {
        voice_->Stop();
        voice_->FlushSourceBuffers();
    }
    isPlaying_ = false;
    isPaused_ = false;
}

void AudioSource::Pause() {
    if (voice_ && isPlaying_ && !isPaused_) {
        voice_->Stop();
        isPaused_ = true;
    }
}

void AudioSource::Resume() {
    if (voice_ && isPlaying_ && isPaused_) {
        voice_->Start();
        isPaused_ = false;
    }
}

bool AudioSource::IsPlaying() const {
    return isPlaying_ && !isPaused_;
}

void AudioSource::SetClip(std::shared_ptr<AudioClip> clip) {
    if (isPlaying_) {
        Stop();
    }

    // 古いボイスを解放
    if (voice_ && audioSystem_) {
        audioSystem_->ReleaseVoice(voice_);
        voice_ = nullptr;
    }

    clip_ = clip;
    if (clip_) {
        clipPath_ = clip_->GetFilePath();
    }
}

bool AudioSource::LoadClip(const std::string& filePath) {
    auto newClip = std::make_shared<AudioClip>();
    if (!newClip->LoadFromFile(filePath)) {
        return false;
    }
    SetClip(newClip);
    return true;
}

void AudioSource::SetVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

void AudioSource::UpdateVolume() {
    if (!voice_) return;

    float finalVolume = volume_;
    if (is3D_) {
        finalVolume *= Calculate3DVolume();
    }
    voice_->SetVolume(finalVolume);
}

float AudioSource::Calculate3DVolume() const {
    if (!gameObject_) return 1.0f;

    // AudioListenerを探す（静的インスタンスから取得）
    AudioListener* listener = AudioListener::GetInstance();
    if (!listener) return 1.0f;

    // 距離計算
    Transform& transform = gameObject_->GetTransform();
    Vector3 sourcePos = transform.GetPosition();

    // エディタオーバーライドがあればそちらを使用
    Vector3 listenerPos;
    if (listener->IsUsingEditorOverride()) {
        listenerPos = listener->GetEditorOverridePosition();
    } else {
        if (!listener->GetGameObject()) return 1.0f;
        listenerPos = listener->GetGameObject()->GetTransform().GetPosition();
    }

    float distance = (sourcePos - listenerPos).Length();

    // 逆二乗減衰
    if (distance <= minDistance_) {
        return 1.0f;
    }
    if (distance >= maxDistance_) {
        return 0.0f;
    }

    float attenuation = minDistance_ / distance;
    return attenuation * attenuation;
}

void AudioSource::SubmitBuffer() {
    if (!voice_ || !clip_) return;

    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(clip_->GetAudioData().size());
    buffer.pAudioData = clip_->GetAudioData().data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    if (loop_) {
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    HRESULT hr = voice_->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) {
        Logger::Error("AudioSource: Failed to submit buffer");
    }
}

} // namespace UnoEngine
