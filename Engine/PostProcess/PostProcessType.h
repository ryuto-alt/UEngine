#pragma once

namespace UnoEngine {

enum class PostProcessType {
    None,
    Grayscale,
    Vignette,
    Fisheye,
    Count
};

inline const char* PostProcessTypeToString(PostProcessType type) {
    switch (type) {
        case PostProcessType::None: return "None";
        case PostProcessType::Grayscale: return "Grayscale";
        case PostProcessType::Vignette: return "Vignette";
        case PostProcessType::Fisheye: return "Fisheye";
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

} // namespace UnoEngine
