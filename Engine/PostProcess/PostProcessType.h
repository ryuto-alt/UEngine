#pragma once

namespace UnoEngine {

enum class PostProcessType {
    None,
    Grayscale,
    // 今後追加予定: Sepia, Vignette, Bloom, etc.
    Count
};

inline const char* PostProcessTypeToString(PostProcessType type) {
    switch (type) {
        case PostProcessType::None: return "None";
        case PostProcessType::Grayscale: return "Grayscale";
        default: return "Unknown";
    }
}

} // namespace UnoEngine
