#pragma once

#include <cstdint>

namespace UnoEngine {

enum class PostProcessType {
    None,
    Grayscale,
    Vignette,
    Fisheye,
    PS1,
    Count
};

inline const char* PostProcessTypeToString(PostProcessType type) {
    switch (type) {
        case PostProcessType::None: return "None";
        case PostProcessType::Grayscale: return "Grayscale";
        case PostProcessType::Vignette: return "Vignette";
        case PostProcessType::Fisheye: return "Fisheye";
        case PostProcessType::PS1: return "PS1";
        default: return "Unknown";
    }
}


// エフェクト固有パラメータ
struct GrayscaleParams {
    float intensity = 1.0f;
};

struct VignetteParams {
    float radius = 0.75f;    // 中央からの減衰開始点
    float softness = 0.45f;  // エッジの柔らかさ
    float intensity = 1.0f;  // 全体の強度
};

struct FisheyeParams {
    float strength = 0.5f;   // 歪みの強さ (0=なし, 1=強い魚眼)
    float zoom = 1.0f;       // ズーム倍率
};

struct PS1Params {
    std::uint32_t colorDepth = 5;   // RGB各チャネルのビット深度 (1-8, PS1は通常5bit)
    float resolutionScale = 4.0f;   // 解像度低下倍率 (1=等倍, 4=1/4解像度)
    bool ditherEnabled = true;      // ディザリング有効
    float ditherStrength = 1.0f;    // ディザリング強度 (0-2)
};

} // namespace UnoEngine
