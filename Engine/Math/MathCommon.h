#pragma once

// Windowsマクロとの競合を回避
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cmath>
#include <algorithm>

namespace UnoEngine {

// シェーダー用の行列構造体（GPUレイアウト互換）
struct Float4x4 {
    float m[4][4];
};

// シェーダー用の3成分ベクトル構造体
struct Float3 {
    float x, y, z;
    Float3() : x(0), y(0), z(0) {}
    Float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

// 定数
namespace Math {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;
    constexpr float HALF_PI = 0.5f * PI;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;
    constexpr float EPSILON = 1e-6f;

    // ユーティリティ関数
    inline float Clamp(float value, float minVal, float maxVal) {
        return std::max(minVal, std::min(maxVal, value));
    }

    inline float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    inline bool NearlyEqual(float a, float b, float epsilon = EPSILON) {
        return std::abs(a - b) < epsilon;
    }
}

} // namespace UnoEngine
