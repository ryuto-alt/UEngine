#pragma once

#include "MathCommon.h"
#include "Vector.h"

namespace UnoEngine {

// Forward declaration
class Matrix4x4;

// クォータニオン
class Quaternion {
public:
    Quaternion() : quat_(DirectX::XMQuaternionIdentity()) {}
    Quaternion(float x, float y, float z, float w) : quat_(DirectX::XMVectorSet(x, y, z, w)) {}
    explicit Quaternion(DirectX::FXMVECTOR q) : quat_(q) {}

    // 要素アクセス
    float GetX() const { return DirectX::XMVectorGetX(quat_); }
    float GetY() const { return DirectX::XMVectorGetY(quat_); }
    float GetZ() const { return DirectX::XMVectorGetZ(quat_); }
    float GetW() const { return DirectX::XMVectorGetW(quat_); }

    // 演算子
    Quaternion operator*(const Quaternion& rhs) const {
        return Quaternion(DirectX::XMQuaternionMultiply(quat_, rhs.quat_));
    }

    Quaternion& operator*=(const Quaternion& rhs) {
        quat_ = DirectX::XMQuaternionMultiply(quat_, rhs.quat_);
        return *this;
    }

    // クォータニオン演算
    Quaternion Normalize() const { return Quaternion(DirectX::XMQuaternionNormalize(quat_)); }
    Quaternion Conjugate() const { return Quaternion(DirectX::XMQuaternionConjugate(quat_)); }
    Quaternion Inverse() const { return Quaternion(DirectX::XMQuaternionInverse(quat_)); }

    float Dot(const Quaternion& rhs) const {
        return DirectX::XMVectorGetX(DirectX::XMQuaternionDot(quat_, rhs.quat_));
    }

    // 補間
    static Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t) {
        return Quaternion(DirectX::XMQuaternionSlerp(q1.quat_, q2.quat_, t));
    }

    // ベクトル回転
    Vector3 RotateVector(const Vector3& vec) const {
        return Vector3(DirectX::XMVector3Rotate(vec.GetXMVector(), quat_));
    }

    Vector3 operator*(const Vector3& vec) const {
        return RotateVector(vec);
    }

    Matrix4x4 ToMatrix() const;

    // DirectXMathへの変換
    DirectX::XMVECTOR GetXMVector() const { return quat_; }

    // 静的メンバ - 生成関数
    static Quaternion Identity() { return Quaternion(); }

    static Quaternion RotationAxis(const Vector3& axis, float radians) {
        return Quaternion(DirectX::XMQuaternionRotationAxis(axis.GetXMVector(), radians));
    }

    static Quaternion RotationRollPitchYaw(float pitch, float yaw, float roll) {
        return Quaternion(DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
    }

    static Quaternion FromRotationMatrix(const Matrix4x4& mat);
    
    static Quaternion LookRotation(const Vector3& forward, const Vector3& up);

private:
    DirectX::XMVECTOR quat_;
};

} // namespace UnoEngine
