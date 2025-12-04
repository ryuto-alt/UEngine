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
    float m[4][4] = {};
    Float4x4() = default;
    Float4x4(float m00, float m01, float m02, float m03,
             float m10, float m11, float m12, float m13,
             float m20, float m21, float m22, float m23,
             float m30, float m31, float m32, float m33) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
        m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
    }
};

// シェーダー用の3成分ベクトル構造体
struct Float3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Float3() = default;
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
