#include "pch.h"
#include "AudioClip.h"
#include "../Core/Logger.h"
#include <fstream>
#include <cstring>

namespace UnoEngine {

// WAVファイルのチャンクヘッダ
struct ChunkHeader {
    char id[4];
    uint32_t size;
};

bool AudioClip::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::Error("AudioClip: Failed to open file: " + filePath);
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<BYTE> fileData(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(fileData.data()), fileSize)) {
        Logger::Error("AudioClip: Failed to read file: " + filePath);
        return false;
    }

    if (!ParseWavFile(fileData)) {
        Logger::Error("AudioClip: Failed to parse WAV file: " + filePath);
        return false;
    }

    filePath_ = filePath;
    isLoaded_ = true;
    Logger::Info("AudioClip: Loaded " + filePath);
    return true;
}

bool AudioClip::ParseWavFile(const std::vector<BYTE>& fileData) {
    if (fileData.size() < 44) {
        Logger::Error("AudioClip: File too small to be a valid WAV");
        return false;
    }

    const BYTE* data = fileData.data();
    size_t offset = 0;

    // RIFFヘッダ確認
    if (std::memcmp(data, "RIFF", 4) != 0) {
        Logger::Error("AudioClip: Invalid RIFF header");
        return false;
    }
    offset += 4;

    // ファイルサイズ（スキップ）
    offset += 4;

    // WAVEフォーマット確認
    if (std::memcmp(data + offset, "WAVE", 4) != 0) {
        Logger::Error("AudioClip: Invalid WAVE format");
        return false;
    }
    offset += 4;

    bool foundFmt = false;
    bool foundData = false;

    // チャンクを読み進める
    while (offset + sizeof(ChunkHeader) <= fileData.size()) {
        ChunkHeader chunk;
        std::memcpy(&chunk, data + offset, sizeof(ChunkHeader));
        offset += sizeof(ChunkHeader);

        if (std::memcmp(chunk.id, "fmt ", 4) == 0) {
            // フォーマットチャンク
            if (chunk.size < 16) {
                Logger::Error("AudioClip: Invalid fmt chunk size");
                return false;
            }

            std::memset(&format_, 0, sizeof(format_));
            std::memcpy(&format_, data + offset, std::min<size_t>(chunk.size, sizeof(format_)));
            format_.cbSize = 0;
            foundFmt = true;
        }
        else if (std::memcmp(chunk.id, "data", 4) == 0) {
            // データチャンク
            if (offset + chunk.size > fileData.size()) {
                Logger::Error("AudioClip: Data chunk exceeds file size");
                return false;
            }

            audioData_.resize(chunk.size);
            std::memcpy(audioData_.data(), data + offset, chunk.size);
            foundData = true;
        }

        // 次のチャンクへ（2バイトアライメント）
        offset += chunk.size;
        if (chunk.size % 2 != 0) offset++;

        if (foundFmt && foundData) break;
    }

    if (!foundFmt || !foundData) {
        Logger::Error("AudioClip: Missing fmt or data chunk");
        return false;
    }

    return true;
}

float AudioClip::GetDuration() const {
    if (!isLoaded_ || format_.nAvgBytesPerSec == 0) return 0.0f;
    return static_cast<float>(audioData_.size()) / static_cast<float>(format_.nAvgBytesPerSec);
}

} // namespace UnoEngine
