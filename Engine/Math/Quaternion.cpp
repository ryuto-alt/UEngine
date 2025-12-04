#include "pch.h"
#include "Quaternion.h"
#include "Matrix.h"
#include <cmath>

namespace UnoEngine {

Quaternion Quaternion::FromRotationMatrix(const Matrix4x4& mat) {
    float m00 = mat.GetElement(0, 0);
    float m01 = mat.GetElement(0, 1);
    float m02 = mat.GetElement(0, 2);
    float m10 = mat.GetElement(1, 0);
    float m11 = mat.GetElement(1, 1);
    float m12 = mat.GetElement(1, 2);
    float m20 = mat.GetElement(2, 0);
    float m21 = mat.GetElement(2, 1);
    float m22 = mat.GetElement(2, 2);

    float trace = m00 + m11 + m22;
    float x, y, z, w;

    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        w = 0.25f * s;
        x = (m12 - m21) / s;
        y = (m20 - m02) / s;
        z = (m01 - m10) / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        w = (m12 - m21) / s;
        x = 0.25f * s;
        y = (m01 + m10) / s;
        z = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        w = (m20 - m02) / s;
        x = (m01 + m10) / s;
        y = 0.25f * s;
        z = (m12 + m21) / s;
    } else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        w = (m01 - m10) / s;
        x = (m02 + m20) / s;
        y = (m12 + m21) / s;
        z = 0.25f * s;
    }

    return Quaternion(x, y, z, w).Normalize();
}

Matrix4x4 Quaternion::ToMatrix() const {
    float xx = x_ * x_;
    float yy = y_ * y_;
    float zz = z_ * z_;
    float xy = x_ * y_;
    float xz = x_ * z_;
    float yz = y_ * z_;
    float wx = w_ * x_;
    float wy = w_ * y_;
    float wz = w_ * z_;

    return Matrix4x4(
        1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),        2.0f * (xz - wy),        0.0f,
        2.0f * (xy - wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),        0.0f,
        2.0f * (xz + wy),        2.0f * (yz - wx),        1.0f - 2.0f * (xx + yy), 0.0f,
        0.0f,                    0.0f,                    0.0f,                    1.0f
    );
}

Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up) {
    Vector3 zAxis = forward.Normalize();
    Vector3 xAxis = up.Cross(zAxis).Normalize();
    Vector3 yAxis = zAxis.Cross(xAxis);

    // 回転行列を構築
    Matrix4x4 rotMat(
        xAxis.GetX(), yAxis.GetX(), zAxis.GetX(), 0.0f,
        xAxis.GetY(), yAxis.GetY(), zAxis.GetY(), 0.0f,
        xAxis.GetZ(), yAxis.GetZ(), zAxis.GetZ(), 0.0f,
        0.0f,         0.0f,         0.0f,         1.0f
    );

    return FromRotationMatrix(rotMat);
}

// Matrix4x4::CreateFromQuaternion の実装
Matrix4x4 Matrix4x4::CreateFromQuaternion(const Quaternion& q) {
    return q.ToMatrix();
}

} // namespace UnoEngine
