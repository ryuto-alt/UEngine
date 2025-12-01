#pragma once

#include <string>
#include <vector>
#include <memory>
#include <xaudio2.h>

namespace UnoEngine {

// WAVファイルをロードして保持するクラス
class AudioClip {
public:
    AudioClip() = default;
    ~AudioClip() = default;

    // WAVファイルをロード
    bool LoadFromFile(const std::string& filePath);

    // アクセサ
    const std::string& GetFilePath() const { return filePath_; }
    const WAVEFORMATEX& GetFormat() const { return format_; }
    const std::vector<BYTE>& GetAudioData() const { return audioData_; }
    bool IsLoaded() const { return isLoaded_; }

    // 再生時間（秒）
    float GetDuration() const;

private:
    bool ParseWavFile(const std::vector<BYTE>& fileData);

    std::string filePath_;
    WAVEFORMATEX format_{};
    std::vector<BYTE> audioData_;
    bool isLoaded_ = false;
};

} // namespace UnoEngine
