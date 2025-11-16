#pragma once

#include <DirectXMath.h>
#include <cmath>

namespace UnoEngine {

// 定数
namespace Math {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;
    constexpr float HALF_PI = 0.5f * PI;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;
    constexpr float EPSILON = 1e-6f;
}

} // namespace UnoEngine
