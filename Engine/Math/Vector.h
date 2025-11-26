#pragma once

#include "MathCommon.h"
#include <string>

namespace UnoEngine {

// 3次元ベクトル
class Vector3 {
public:
    Vector3() : vec_(DirectX::XMVectorZero()) {}
    Vector3(float x, float y, float z) : vec_(DirectX::XMVectorSet(x, y, z, 0.0f)) {}
    explicit Vector3(DirectX::FXMVECTOR v) : vec_(v) {}

    // 要素アクセス
    float GetX() const { return DirectX::XMVectorGetX(vec_); }
    float GetY() const { return DirectX::XMVectorGetY(vec_); }
    float GetZ() const { return DirectX::XMVectorGetZ(vec_); }

    void SetX(float x) { vec_ = DirectX::XMVectorSetX(vec_, x); }
    void SetY(float y) { vec_ = DirectX::XMVectorSetY(vec_, y); }
    void SetZ(float z) { vec_ = DirectX::XMVectorSetZ(vec_, z); }

    // 演算子
    Vector3 operator+(const Vector3& rhs) const { return Vector3(DirectX::XMVectorAdd(vec_, rhs.vec_)); }
    Vector3 operator-(const Vector3& rhs) const { return Vector3(DirectX::XMVectorSubtract(vec_, rhs.vec_)); }
    Vector3 operator*(float scalar) const { return Vector3(DirectX::XMVectorScale(vec_, scalar)); }
    Vector3 operator/(float scalar) const { return Vector3(DirectX::XMVectorScale(vec_, 1.0f / scalar)); }

    Vector3& operator+=(const Vector3& rhs) { vec_ = DirectX::XMVectorAdd(vec_, rhs.vec_); return *this; }
    Vector3& operator-=(const Vector3& rhs) { vec_ = DirectX::XMVectorSubtract(vec_, rhs.vec_); return *this; }
    Vector3& operator*=(float scalar) { vec_ = DirectX::XMVectorScale(vec_, scalar); return *this; }
    Vector3& operator/=(float scalar) { vec_ = DirectX::XMVectorScale(vec_, 1.0f / scalar); return *this; }

    // ベクトル演算
    float Length() const { return DirectX::XMVectorGetX(DirectX::XMVector3Length(vec_)); }
    float LengthSq() const { return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(vec_)); }
    Vector3 Normalize() const { return Vector3(DirectX::XMVector3Normalize(vec_)); }
    float Dot(const Vector3& rhs) const { return DirectX::XMVectorGetX(DirectX::XMVector3Dot(vec_, rhs.vec_)); }
    Vector3 Cross(const Vector3& rhs) const { return Vector3(DirectX::XMVector3Cross(vec_, rhs.vec_)); }

    // DirectXMathへの変換
    DirectX::XMVECTOR GetXMVector() const { return vec_; }

    // 静的メンバ
    static Vector3 Zero() { return Vector3(0.0f, 0.0f, 0.0f); }
    static Vector3 One() { return Vector3(1.0f, 1.0f, 1.0f); }
    static Vector3 UnitX() { return Vector3(1.0f, 0.0f, 0.0f); }
    static Vector3 UnitY() { return Vector3(0.0f, 1.0f, 0.0f); }
    static Vector3 UnitZ() { return Vector3(0.0f, 0.0f, 1.0f); }

    // 線形補間
    static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
        return Vector3(DirectX::XMVectorLerp(a.vec_, b.vec_, t));
    }

private:
    DirectX::XMVECTOR vec_;
};

// 4次元ベクトル
class Vector4 {
public:
    Vector4() : vec_(DirectX::XMVectorZero()) {}
    Vector4(float x, float y, float z, float w) : vec_(DirectX::XMVectorSet(x, y, z, w)) {}
    explicit Vector4(DirectX::FXMVECTOR v) : vec_(v) {}

    // 要素アクセス
    float GetX() const { return DirectX::XMVectorGetX(vec_); }
    float GetY() const { return DirectX::XMVectorGetY(vec_); }
    float GetZ() const { return DirectX::XMVectorGetZ(vec_); }
    float GetW() const { return DirectX::XMVectorGetW(vec_); }

    void SetX(float x) { vec_ = DirectX::XMVectorSetX(vec_, x); }
    void SetY(float y) { vec_ = DirectX::XMVectorSetY(vec_, y); }
    void SetZ(float z) { vec_ = DirectX::XMVectorSetZ(vec_, z); }
    void SetW(float w) { vec_ = DirectX::XMVectorSetW(vec_, w); }

    // 演算子
    Vector4 operator+(const Vector4& rhs) const { return Vector4(DirectX::XMVectorAdd(vec_, rhs.vec_)); }
    Vector4 operator-(const Vector4& rhs) const { return Vector4(DirectX::XMVectorSubtract(vec_, rhs.vec_)); }
    Vector4 operator*(float scalar) const { return Vector4(DirectX::XMVectorScale(vec_, scalar)); }
    Vector4 operator/(float scalar) const { return Vector4(DirectX::XMVectorScale(vec_, 1.0f / scalar)); }

    // DirectXMathへの変換
    DirectX::XMVECTOR GetXMVector() const { return vec_; }

    // 静的メンバ
    static Vector4 Zero() { return Vector4(0.0f, 0.0f, 0.0f, 0.0f); }

private:
    DirectX::XMVECTOR vec_;
};

// 2次元ベクトル（UV座標用）
class Vector2 {
public:
    Vector2() : x(0.0f), y(0.0f) {}
    Vector2(float x, float y) : x(x), y(y) {}

    float GetX() const { return x; }
    float GetY() const { return y; }
    void SetX(float value) { x = value; }
    void SetY(float value) { y = value; }

    static Vector2 Zero() { return Vector2(0.0f, 0.0f); }
    static Vector2 One() { return Vector2(1.0f, 1.0f); }

    float x, y;
};

// スカラー * ベクトル
inline Vector3 operator*(float scalar, const Vector3& vec) { return vec * scalar; }
inline Vector4 operator*(float scalar, const Vector4& vec) { return vec * scalar; }

} // namespace UnoEngine
