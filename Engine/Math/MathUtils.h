#pragma once

#include "MathCommon.h"
#include <algorithm>

namespace UnoEngine {
namespace Math {

// 角度変換
inline float ToRadians(float degrees) {
    return degrees * DEG_TO_RAD;
}

inline float ToDegrees(float radians) {
    return radians * RAD_TO_DEG;
}

// クランプ
template<typename T>
inline T Clamp(T value, T min, T max) {
    return std::max(min, std::min(value, max));
}

// 線形補間
inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// 正規化された値（0-1）
inline float Saturate(float value) {
    return Clamp(value, 0.0f, 1.0f);
}

// 浮動小数点比較
inline bool NearlyEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

// 符号取得
template<typename T>
inline T Sign(T value) {
    return (T(0) < value) - (value < T(0));
}

// 最小・最大
template<typename T>
inline T Min(T a, T b) {
    return std::min(a, b);
}

template<typename T>
inline T Max(T a, T b) {
    return std::max(a, b);
}

// 絶対値
template<typename T>
inline T Abs(T value) {
    return std::abs(value);
}

// 平滑補間（Smoothstep）
inline float Smoothstep(float edge0, float edge1, float x) {
    float t = Saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

} // namespace Math
} // namespace UnoEngine
