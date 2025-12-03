#pragma once

#include "Vector.h"
#include <cmath>

namespace UnoEngine {

// Forward declaration
class Matrix4x4;

// クォータニオン
class Quaternion {
public:
    // デフォルトコンストラクタ（単位クォータニオン）
    Quaternion() : x_(0.0f), y_(0.0f), z_(0.0f), w_(1.0f) {}
    Quaternion(float x, float y, float z, float w) : x_(x), y_(y), z_(z), w_(w) {}

    // 要素アクセス
    float GetX() const { return x_; }
    float GetY() const { return y_; }
    float GetZ() const { return z_; }
    float GetW() const { return w_; }

    void SetX(float x) { x_ = x; }
    void SetY(float y) { y_ = y; }
    void SetZ(float z) { z_ = z; }
    void SetW(float w) { w_ = w; }

    // クォータニオン乗算
    Quaternion operator*(const Quaternion& rhs) const {
        return Quaternion(
            w_ * rhs.x_ + x_ * rhs.w_ + y_ * rhs.z_ - z_ * rhs.y_,
            w_ * rhs.y_ - x_ * rhs.z_ + y_ * rhs.w_ + z_ * rhs.x_,
            w_ * rhs.z_ + x_ * rhs.y_ - y_ * rhs.x_ + z_ * rhs.w_,
            w_ * rhs.w_ - x_ * rhs.x_ - y_ * rhs.y_ - z_ * rhs.z_
        );
    }

    Quaternion& operator*=(const Quaternion& rhs) {
        *this = *this * rhs;
        return *this;
    }

    // 長さ
    float Length() const {
        return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_ + w_ * w_);
    }

    float LengthSq() const {
        return x_ * x_ + y_ * y_ + z_ * z_ + w_ * w_;
    }

    // 正規化
    Quaternion Normalize() const {
        float len = Length();
        if (len > 0.0f) {
            float invLen = 1.0f / len;
            return Quaternion(x_ * invLen, y_ * invLen, z_ * invLen, w_ * invLen);
        }
        return *this;
    }

    // 共役
    Quaternion Conjugate() const {
        return Quaternion(-x_, -y_, -z_, w_);
    }

    // 逆クォータニオン
    Quaternion Inverse() const {
        float lenSq = LengthSq();
        if (lenSq > 0.0f) {
            float invLenSq = 1.0f / lenSq;
            return Quaternion(-x_ * invLenSq, -y_ * invLenSq, -z_ * invLenSq, w_ * invLenSq);
        }
        return *this;
    }

    // 内積
    float Dot(const Quaternion& rhs) const {
        return x_ * rhs.x_ + y_ * rhs.y_ + z_ * rhs.z_ + w_ * rhs.w_;
    }

    // 球面線形補間
    static Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t) {
        float dot = q1.Dot(q2);
        
        Quaternion q2Adj = q2;
        if (dot < 0.0f) {
            dot = -dot;
            q2Adj = Quaternion(-q2.x_, -q2.y_, -q2.z_, -q2.w_);
        }

        if (dot > 0.9995f) {
            // 角度が小さい場合は線形補間
            return Quaternion(
                q1.x_ + t * (q2Adj.x_ - q1.x_),
                q1.y_ + t * (q2Adj.y_ - q1.y_),
                q1.z_ + t * (q2Adj.z_ - q1.z_),
                q1.w_ + t * (q2Adj.w_ - q1.w_)
            ).Normalize();
        }

        float theta0 = std::acos(dot);
        float theta = theta0 * t;
        float sinTheta = std::sin(theta);
        float sinTheta0 = std::sin(theta0);

        float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;

        return Quaternion(
            s0 * q1.x_ + s1 * q2Adj.x_,
            s0 * q1.y_ + s1 * q2Adj.y_,
            s0 * q1.z_ + s1 * q2Adj.z_,
            s0 * q1.w_ + s1 * q2Adj.w_
        );
    }

    // ベクトル回転
    Vector3 RotateVector(const Vector3& vec) const {
        // q * v * q^-1
        Quaternion v(vec.GetX(), vec.GetY(), vec.GetZ(), 0.0f);
        Quaternion result = (*this) * v * Conjugate();
        return Vector3(result.x_, result.y_, result.z_);
    }

    Vector3 operator*(const Vector3& vec) const {
        return RotateVector(vec);
    }

    // 行列への変換
    Matrix4x4 ToMatrix() const;

    // 静的メンバ - 生成関数
    static Quaternion Identity() { return Quaternion(); }

    // 任意軸回転
    static Quaternion RotationAxis(const Vector3& axis, float radians) {
        Vector3 n = axis.Normalize();
        float halfAngle = radians * 0.5f;
        float s = std::sin(halfAngle);
        return Quaternion(
            n.GetX() * s,
            n.GetY() * s,
            n.GetZ() * s,
            std::cos(halfAngle)
        );
    }

    // オイラー角から生成（ピッチ=X, ヨー=Y, ロール=Z）
    // 回転順序: Z(roll) * X(pitch) * Y(yaw)
    static Quaternion RotationRollPitchYaw(float pitch, float yaw, float roll) {
        float cp = std::cos(pitch * 0.5f);
        float sp = std::sin(pitch * 0.5f);
        float cy = std::cos(yaw * 0.5f);
        float sy = std::sin(yaw * 0.5f);
        float cr = std::cos(roll * 0.5f);
        float sr = std::sin(roll * 0.5f);

        return Quaternion(
            sr * cp * cy - cr * sp * sy,  // x
            cr * sp * cy + sr * cp * sy,  // y
            cr * cp * sy - sr * sp * cy,  // z
            cr * cp * cy + sr * sp * sy   // w
        );
    }

    // 回転行列から生成
    static Quaternion FromRotationMatrix(const Matrix4x4& mat);
    
    // 方向ベクトルから回転を生成
    static Quaternion LookRotation(const Vector3& forward, const Vector3& up);

private:
    float x_, y_, z_, w_;
};

} // namespace UnoEngine
