#pragma once

#include <cmath>

namespace UnoEngine {

// 2次元ベクトル（UV座標用）
class Vector2 {
public:
    Vector2() : x_(0.0f), y_(0.0f) {}
    Vector2(float x, float y) : x_(x), y_(y) {}

    // 要素アクセス
    float GetX() const { return x_; }
    float GetY() const { return y_; }
    void SetX(float value) { x_ = value; }
    void SetY(float value) { y_ = value; }

    // 演算子
    Vector2 operator+(const Vector2& rhs) const { return Vector2(x_ + rhs.x_, y_ + rhs.y_); }
    Vector2 operator-(const Vector2& rhs) const { return Vector2(x_ - rhs.x_, y_ - rhs.y_); }
    Vector2 operator*(float scalar) const { return Vector2(x_ * scalar, y_ * scalar); }
    Vector2 operator/(float scalar) const { float inv = 1.0f / scalar; return Vector2(x_ * inv, y_ * inv); }
    Vector2 operator-() const { return Vector2(-x_, -y_); }

    Vector2& operator+=(const Vector2& rhs) { x_ += rhs.x_; y_ += rhs.y_; return *this; }
    Vector2& operator-=(const Vector2& rhs) { x_ -= rhs.x_; y_ -= rhs.y_; return *this; }
    Vector2& operator*=(float scalar) { x_ *= scalar; y_ *= scalar; return *this; }
    Vector2& operator/=(float scalar) { float inv = 1.0f / scalar; x_ *= inv; y_ *= inv; return *this; }

    bool operator==(const Vector2& rhs) const { return x_ == rhs.x_ && y_ == rhs.y_; }
    bool operator!=(const Vector2& rhs) const { return !(*this == rhs); }

    // ベクトル演算
    float Length() const { return std::sqrt(x_ * x_ + y_ * y_); }
    float LengthSq() const { return x_ * x_ + y_ * y_; }
    Vector2 Normalize() const {
        float len = Length();
        if (len > 0.0f) {
            float inv = 1.0f / len;
            return Vector2(x_ * inv, y_ * inv);
        }
        return *this;
    }
    float Dot(const Vector2& rhs) const { return x_ * rhs.x_ + y_ * rhs.y_; }

    // 静的メンバ
    static Vector2 Zero() { return Vector2(0.0f, 0.0f); }
    static Vector2 One() { return Vector2(1.0f, 1.0f); }
    static Vector2 UnitX() { return Vector2(1.0f, 0.0f); }
    static Vector2 UnitY() { return Vector2(0.0f, 1.0f); }

    // 線形補間
    static Vector2 Lerp(const Vector2& a, const Vector2& b, float t) {
        return Vector2(a.x_ + (b.x_ - a.x_) * t, a.y_ + (b.y_ - a.y_) * t);
    }

private:
    float x_, y_;
};

// 3次元ベクトル
class Vector3 {
public:
    Vector3() : x_(0.0f), y_(0.0f), z_(0.0f) {}
    Vector3(float x, float y, float z) : x_(x), y_(y), z_(z) {}

    // 要素アクセス
    float GetX() const { return x_; }
    float GetY() const { return y_; }
    float GetZ() const { return z_; }

    void SetX(float x) { x_ = x; }
    void SetY(float y) { y_ = y; }
    void SetZ(float z) { z_ = z; }

    // 演算子
    Vector3 operator+(const Vector3& rhs) const { return Vector3(x_ + rhs.x_, y_ + rhs.y_, z_ + rhs.z_); }
    Vector3 operator-(const Vector3& rhs) const { return Vector3(x_ - rhs.x_, y_ - rhs.y_, z_ - rhs.z_); }
    Vector3 operator*(float scalar) const { return Vector3(x_ * scalar, y_ * scalar, z_ * scalar); }
    Vector3 operator/(float scalar) const { float inv = 1.0f / scalar; return Vector3(x_ * inv, y_ * inv, z_ * inv); }
    Vector3 operator-() const { return Vector3(-x_, -y_, -z_); }

    Vector3& operator+=(const Vector3& rhs) { x_ += rhs.x_; y_ += rhs.y_; z_ += rhs.z_; return *this; }
    Vector3& operator-=(const Vector3& rhs) { x_ -= rhs.x_; y_ -= rhs.y_; z_ -= rhs.z_; return *this; }
    Vector3& operator*=(float scalar) { x_ *= scalar; y_ *= scalar; z_ *= scalar; return *this; }
    Vector3& operator/=(float scalar) { float inv = 1.0f / scalar; x_ *= inv; y_ *= inv; z_ *= inv; return *this; }

    bool operator==(const Vector3& rhs) const { return x_ == rhs.x_ && y_ == rhs.y_ && z_ == rhs.z_; }
    bool operator!=(const Vector3& rhs) const { return !(*this == rhs); }

    // ベクトル演算
    float Length() const { return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_); }
    float LengthSq() const { return x_ * x_ + y_ * y_ + z_ * z_; }

    Vector3 Normalize() const {
        float len = Length();
        if (len > 0.0f) {
            float inv = 1.0f / len;
            return Vector3(x_ * inv, y_ * inv, z_ * inv);
        }
        return *this;
    }

    float Dot(const Vector3& rhs) const { return x_ * rhs.x_ + y_ * rhs.y_ + z_ * rhs.z_; }

    Vector3 Cross(const Vector3& rhs) const {
        return Vector3(
            y_ * rhs.z_ - z_ * rhs.y_,
            z_ * rhs.x_ - x_ * rhs.z_,
            x_ * rhs.y_ - y_ * rhs.x_
        );
    }

    // 静的メンバ
    static Vector3 Zero() { return Vector3(0.0f, 0.0f, 0.0f); }
    static Vector3 One() { return Vector3(1.0f, 1.0f, 1.0f); }
    static Vector3 UnitX() { return Vector3(1.0f, 0.0f, 0.0f); }
    static Vector3 UnitY() { return Vector3(0.0f, 1.0f, 0.0f); }
    static Vector3 UnitZ() { return Vector3(0.0f, 0.0f, 1.0f); }

    // 線形補間
    static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
        return Vector3(
            a.x_ + (b.x_ - a.x_) * t,
            a.y_ + (b.y_ - a.y_) * t,
            a.z_ + (b.z_ - a.z_) * t
        );
    }

private:
    float x_, y_, z_;
};

// 4次元ベクトル
class Vector4 {
public:
    Vector4() : x_(0.0f), y_(0.0f), z_(0.0f), w_(0.0f) {}
    Vector4(float x, float y, float z, float w) : x_(x), y_(y), z_(z), w_(w) {}

    // 要素アクセス
    float GetX() const { return x_; }
    float GetY() const { return y_; }
    float GetZ() const { return z_; }
    float GetW() const { return w_; }

    void SetX(float x) { x_ = x; }
    void SetY(float y) { y_ = y; }
    void SetZ(float z) { z_ = z; }
    void SetW(float w) { w_ = w; }

    // 演算子
    Vector4 operator+(const Vector4& rhs) const { return Vector4(x_ + rhs.x_, y_ + rhs.y_, z_ + rhs.z_, w_ + rhs.w_); }
    Vector4 operator-(const Vector4& rhs) const { return Vector4(x_ - rhs.x_, y_ - rhs.y_, z_ - rhs.z_, w_ - rhs.w_); }
    Vector4 operator*(float scalar) const { return Vector4(x_ * scalar, y_ * scalar, z_ * scalar, w_ * scalar); }
    Vector4 operator/(float scalar) const { float inv = 1.0f / scalar; return Vector4(x_ * inv, y_ * inv, z_ * inv, w_ * inv); }
    Vector4 operator-() const { return Vector4(-x_, -y_, -z_, -w_); }

    Vector4& operator+=(const Vector4& rhs) { x_ += rhs.x_; y_ += rhs.y_; z_ += rhs.z_; w_ += rhs.w_; return *this; }
    Vector4& operator-=(const Vector4& rhs) { x_ -= rhs.x_; y_ -= rhs.y_; z_ -= rhs.z_; w_ -= rhs.w_; return *this; }
    Vector4& operator*=(float scalar) { x_ *= scalar; y_ *= scalar; z_ *= scalar; w_ *= scalar; return *this; }
    Vector4& operator/=(float scalar) { float inv = 1.0f / scalar; x_ *= inv; y_ *= inv; z_ *= inv; w_ *= inv; return *this; }

    bool operator==(const Vector4& rhs) const { return x_ == rhs.x_ && y_ == rhs.y_ && z_ == rhs.z_ && w_ == rhs.w_; }
    bool operator!=(const Vector4& rhs) const { return !(*this == rhs); }

    // ベクトル演算
    float Length() const { return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_ + w_ * w_); }
    float LengthSq() const { return x_ * x_ + y_ * y_ + z_ * z_ + w_ * w_; }

    Vector4 Normalize() const {
        float len = Length();
        if (len > 0.0f) {
            float inv = 1.0f / len;
            return Vector4(x_ * inv, y_ * inv, z_ * inv, w_ * inv);
        }
        return *this;
    }

    float Dot(const Vector4& rhs) const { return x_ * rhs.x_ + y_ * rhs.y_ + z_ * rhs.z_ + w_ * rhs.w_; }

    // 静的メンバ
    static Vector4 Zero() { return Vector4(0.0f, 0.0f, 0.0f, 0.0f); }
    static Vector4 One() { return Vector4(1.0f, 1.0f, 1.0f, 1.0f); }

    // 線形補間
    static Vector4 Lerp(const Vector4& a, const Vector4& b, float t) {
        return Vector4(
            a.x_ + (b.x_ - a.x_) * t,
            a.y_ + (b.y_ - a.y_) * t,
            a.z_ + (b.z_ - a.z_) * t,
            a.w_ + (b.w_ - a.w_) * t
        );
    }

private:
    float x_, y_, z_, w_;
};

// スカラー * ベクトル
inline Vector2 operator*(float scalar, const Vector2& vec) { return vec * scalar; }
inline Vector3 operator*(float scalar, const Vector3& vec) { return vec * scalar; }
inline Vector4 operator*(float scalar, const Vector4& vec) { return vec * scalar; }

} // namespace UnoEngine
