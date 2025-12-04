#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 基本型エイリアス
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

// Float3, Float4x4はMathCommon.hで定義されている
#include "../Math/MathCommon.h"

namespace UnoEngine {

// シンプルな浮動小数点ベクトル構造体（GPU/シリアライズ用）
struct Float2 {
    float x = 0.0f, y = 0.0f;
    Float2() = default;
    Float2(float x_, float y_) : x(x_), y(y_) {}
};

struct Float4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    Float4() = default;
    Float4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

} // namespace UnoEngine

// スマートポインタエイリアス
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T, typename... Args>
constexpr UniquePtr<T> MakeUnique(Args&&... args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
constexpr SharedPtr<T> MakeShared(Args&&... args) {
	return std::make_shared<T>(std::forward<Args>(args)...);
}
