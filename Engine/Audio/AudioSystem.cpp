#include "pch.h"
#include "AudioSystem.h"
#include "../Core/Scene.h"
#include "../Core/Logger.h"
#include <algorithm>

#pragma comment(lib, "xaudio2.lib")

namespace UnoEngine {

AudioSystem* AudioSystem::instance_ = nullptr;

AudioSystem::AudioSystem() {
    instance_ = this;
    Initialize();
}

AudioSystem::~AudioSystem() {
    Shutdown();
}

bool AudioSystem::Initialize() {
    if (initialized_) return true;

    // COM初期化（XAudio2はCOMベース）
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        Logger::Error("Failed to initialize COM for XAudio2");
        return false;
    }

    // XAudio2インスタンス作成
    hr = XAudio2Create(xaudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        Logger::Error("Failed to create XAudio2 instance");
        return false;
    }

    // マスターボイス作成
    hr = xaudio2_->CreateMasteringVoice(&masterVoice_);
    if (FAILED(hr)) {
        Logger::Error("Failed to create mastering voice");
        xaudio2_.Reset();
        return false;
    }

    // マスターボイスの詳細を取得
    XAUDIO2_VOICE_DETAILS voiceDetails;
    masterVoice_->GetVoiceDetails(&voiceDetails);
    outputChannels_ = voiceDetails.InputChannels;
    Logger::Info("AudioSystem: Output channels = " + std::to_string(outputChannels_));

    // X3DAudio初期化
    DWORD channelMask;
    hr = masterVoice_->GetChannelMask(&channelMask);
    if (SUCCEEDED(hr)) {
        Logger::Info("AudioSystem: Channel mask = " + std::to_string(channelMask));
        hr = X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, x3dAudioHandle_);
        if (SUCCEEDED(hr)) {
            x3dAudioInitialized_ = true;
            Logger::Info("X3DAudio initialized successfully");
        } else {
            Logger::Warning("Failed to initialize X3DAudio (hr=" + std::to_string(hr) + "), 3D audio will be disabled");
        }
    } else {
        Logger::Warning("Failed to get channel mask (hr=" + std::to_string(hr) + "), 3D audio will be disabled");
    }

    initialized_ = true;
    Logger::Info("AudioSystem initialized (XAudio2)");
    return true;
}

void AudioSystem::Shutdown() {
    if (!initialized_) return;

    if (instance_ == this) {
        instance_ = nullptr;
    }

    // 全ボイスを停止・解放
    for (auto* voice : activeVoices_) {
        if (voice) {
            voice->Stop();
            voice->DestroyVoice();
        }
    }
    activeVoices_.clear();

    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }

    xaudio2_.Reset();
    initialized_ = false;
    Logger::Info("AudioSystem shutdown");
}

void AudioSystem::OnSceneStart(Scene* scene) {
    // シーン開始時の処理（必要に応じて）
}

void AudioSystem::OnUpdate(Scene* scene, float deltaTime) {
    // 3Dオーディオの更新はAudioSourceのOnUpdateで行う
}

void AudioSystem::OnSceneEnd(Scene* scene) {
    // シーン終了時に全ボイスを停止
    for (auto* voice : activeVoices_) {
        if (voice) {
            voice->Stop();
        }
    }
}

IXAudio2SourceVoice* AudioSystem::AcquireVoice(const WAVEFORMATEX* format) {
    if (!initialized_ || !format) return nullptr;

    if (activeVoices_.size() >= MAX_VOICES) {
        Logger::Warning("AudioSystem: Maximum voice limit reached");
        return nullptr;
    }

    IXAudio2SourceVoice* voice = nullptr;
    HRESULT hr = xaudio2_->CreateSourceVoice(&voice, format);
    if (FAILED(hr)) {
        Logger::Error("Failed to create source voice");
        return nullptr;
    }

    activeVoices_.push_back(voice);
    return voice;
}

void AudioSystem::ReleaseVoice(IXAudio2SourceVoice* voice) {
    if (!voice) return;

    auto it = std::find(activeVoices_.begin(), activeVoices_.end(), voice);
    if (it != activeVoices_.end()) {
        voice->Stop();
        voice->DestroyVoice();
        activeVoices_.erase(it);
    }
}

void AudioSystem::PauseAll() {
    if (isPaused_) return;

    for (auto* voice : activeVoices_) {
        if (voice) {
            voice->Stop();
        }
    }
    isPaused_ = true;
}

void AudioSystem::ResumeAll() {
    if (!isPaused_) return;

    for (auto* voice : activeVoices_) {
        if (voice) {
            voice->Start();
        }
    }
    isPaused_ = false;
}

} // namespace UnoEngine
