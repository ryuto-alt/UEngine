#include "pch.h"
#include "AudioSource.h"
#include "AudioClip.h"
#include "AudioSystem.h"
#include "AudioListener.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"
#include "../Core/Logger.h"
#include <cmath>
#include <algorithm>

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

    // 3Dオーディオの更新
    if (is3D_) {
        Update3DAudio();
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
    CleanupDSPSettings();
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

    // 3D用のDSP設定を初期化
    if (is3D_ && audioSystem_->IsX3DAudioInitialized()) {
        InitializeDSPSettings();
    }

    SubmitBuffer();
    
    // 初期音量設定
    if (is3D_) {
        Update3DAudio();
    } else {
        voice_->SetVolume(volume_);
    }
    
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

    CleanupDSPSettings();

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
    if (!is3D_ && voice_) {
        voice_->SetVolume(volume_);
    }
}

void AudioSource::InitializeDSPSettings() {
    if (!audioSystem_ || dspInitialized_ || !clip_) return;

    UINT32 outputChannels = audioSystem_->GetOutputChannels();
    if (outputChannels == 0) outputChannels = 2; // デフォルトはステレオ

    // 実際のクリップのチャンネル数を使用
    UINT32 sourceChannels = clip_->GetFormat().nChannels;
    
    Logger::Info("AudioSource: Initializing 3D audio - Source channels: " + std::to_string(sourceChannels) + 
                 ", Output channels: " + std::to_string(outputChannels));

    // マトリックス係数のメモリを確保
    matrixCoefficients_.resize(sourceChannels * outputChannels, 0.0f);

    // DSP設定を初期化
    memset(&dspSettings_, 0, sizeof(dspSettings_));
    dspSettings_.SrcChannelCount = sourceChannels;
    dspSettings_.DstChannelCount = outputChannels;
    dspSettings_.pMatrixCoefficients = matrixCoefficients_.data();

    // エミッター設定を初期化
    memset(&emitter_, 0, sizeof(emitter_));
    emitter_.ChannelCount = sourceChannels;
    emitter_.CurveDistanceScaler = (minDistance_ > 0.0f) ? minDistance_ : 1.0f;
    emitter_.DopplerScaler = 1.0f;
    
    // 内部半径（これより近いと立体音響効果が弱まる）
    emitter_.InnerRadius = 0.0f;
    emitter_.InnerRadiusAngle = X3DAUDIO_PI / 4.0f;
    
    // チャンネルアジマス（ステレオソースの場合に必要）
    if (sourceChannels > 1) {
        // ステレオソースの場合、左右チャンネルの角度を設定
        static float channelAzimuths[2] = { -X3DAUDIO_PI / 4.0f, X3DAUDIO_PI / 4.0f };
        emitter_.pChannelAzimuths = channelAzimuths;
        emitter_.ChannelRadius = 0.1f;
    }

    dspInitialized_ = true;
}

void AudioSource::CleanupDSPSettings() {
    matrixCoefficients_.clear();
    dspInitialized_ = false;
}

void AudioSource::Update3DAudio() {
    if (!voice_ || !audioSystem_) {
        Logger::Warning("AudioSource::Update3DAudio: voice or audioSystem is null");
        return;
    }
    
    if (!audioSystem_->IsX3DAudioInitialized()) {
        Logger::Warning("AudioSource::Update3DAudio: X3DAudio not initialized, falling back to volume-only");
        // X3DAudioが初期化されていない場合は単純な距離減衰のみ
        AudioListener* listener = AudioListener::GetInstance();
        if (listener && gameObject_) {
            Vector3 listenerPos = listener->GetListenerPosition();
            Vector3 sourcePos = gameObject_->GetTransform().GetPosition();
            float distance = (sourcePos - listenerPos).Length();
            
            float attenuation = 1.0f;
            if (distance >= maxDistance_) {
                attenuation = 0.0f;
            } else if (distance > minDistance_) {
                float normalizedDist = (distance - minDistance_) / (maxDistance_ - minDistance_);
                attenuation = 1.0f - normalizedDist;
            }
            voice_->SetVolume(volume_ * attenuation);
        }
        return;
    }

    AudioListener* listener = AudioListener::GetInstance();
    if (!listener) {
        Logger::Warning("AudioSource::Update3DAudio: No AudioListener in scene");
        voice_->SetVolume(volume_);
        return;
    }

    if (!dspInitialized_) {
        InitializeDSPSettings();
    }

    // リスナー情報を取得
    Vector3 listenerPos = listener->GetListenerPosition();
    Vector3 listenerForward = listener->GetListenerForward();
    Vector3 listenerUp = listener->GetListenerUp();

    // X3DAudio用のリスナー構造体を設定
    X3DAUDIO_LISTENER x3dListener = {};
    // X3DAudioは左手座標系を使用するため、Z軸を反転しない
    x3dListener.Position.x = listenerPos.GetX();
    x3dListener.Position.y = listenerPos.GetY();
    x3dListener.Position.z = listenerPos.GetZ();
    x3dListener.OrientFront.x = listenerForward.GetX();
    x3dListener.OrientFront.y = listenerForward.GetY();
    x3dListener.OrientFront.z = listenerForward.GetZ();
    x3dListener.OrientTop.x = listenerUp.GetX();
    x3dListener.OrientTop.y = listenerUp.GetY();
    x3dListener.OrientTop.z = listenerUp.GetZ();

    // エミッター位置を更新
    if (gameObject_) {
        Vector3 sourcePos = gameObject_->GetTransform().GetPosition();
        emitter_.Position.x = sourcePos.GetX();
        emitter_.Position.y = sourcePos.GetY();
        emitter_.Position.z = sourcePos.GetZ();
    }

    // CurveDistanceScalerを更新（0より大きい値である必要がある）
    emitter_.CurveDistanceScaler = (minDistance_ > 0.0f) ? minDistance_ : 1.0f;

    // X3DAudioで計算
    UINT32 flags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER;
    X3DAudioCalculate(
        audioSystem_->GetX3DAudioHandle(),
        &x3dListener,
        &emitter_,
        flags,
        &dspSettings_
    );
    
    // デバッグ: 計算結果を確認（最初の数回だけログ出力）
    static int debugCount = 0;
    if (debugCount < 10) {
        Logger::Info("3DAudio: Listener(" + std::to_string(listenerPos.GetX()) + "," + 
                     std::to_string(listenerPos.GetY()) + "," + std::to_string(listenerPos.GetZ()) + 
                     ") Emitter(" + std::to_string(emitter_.Position.x) + "," + 
                     std::to_string(emitter_.Position.y) + "," + std::to_string(emitter_.Position.z) + 
                     ") Matrix[0]=" + std::to_string(dspSettings_.pMatrixCoefficients[0]) +
                     " Matrix[1]=" + std::to_string(dspSettings_.pMatrixCoefficients[1]));
        debugCount++;
    }

    // 距離による減衰を計算（X3DAudioのデフォルト減衰に加えて最大距離でカットオフ）
    Vector3 sourcePos = gameObject_ ? gameObject_->GetTransform().GetPosition() : Vector3::Zero();
    float distance = (sourcePos - listenerPos).Length();
    
    float distanceAttenuation = 1.0f;
    if (distance >= maxDistance_) {
        distanceAttenuation = 0.0f;
    } else if (distance > minDistance_) {
        // 最大距離に近づくにつれて減衰
        float normalizedDist = (distance - minDistance_) / (maxDistance_ - minDistance_);
        distanceAttenuation = 1.0f - normalizedDist;
    }

    // マトリックス係数にボリュームと距離減衰を適用
    for (size_t i = 0; i < matrixCoefficients_.size(); ++i) {
        matrixCoefficients_[i] = dspSettings_.pMatrixCoefficients[i] * volume_ * distanceAttenuation;
    }

    // 出力マトリックスを設定（パンニングを適用）
    voice_->SetOutputMatrix(
        audioSystem_->GetMasterVoice(),
        dspSettings_.SrcChannelCount,
        dspSettings_.DstChannelCount,
        matrixCoefficients_.data()
    );

    // ドップラー効果（ピッチ変更）
    if (dspSettings_.DopplerFactor > 0.0f) {
        voice_->SetFrequencyRatio(dspSettings_.DopplerFactor);
    }
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
