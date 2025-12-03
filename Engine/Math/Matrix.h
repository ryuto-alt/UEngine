#pragma once

#include "Vector.h"
#include "../Core/Types.h"
#include <cmath>
#include <cstring>

namespace UnoEngine {

class Quaternion;

// 4x4行列 (行優先 row-major)
class Matrix4x4 {
public:
    // デフォルトコンストラクタ（単位行列）
    Matrix4x4() {
        std::memset(m_, 0, sizeof(m_));
        m_[0][0] = m_[1][1] = m_[2][2] = m_[3][3] = 1.0f;
    }

    // 16要素コンストラクタ (行優先)
    Matrix4x4(float m00, float m01, float m02, float m03,
              float m10, float m11, float m12, float m13,
              float m20, float m21, float m22, float m23,
              float m30, float m31, float m32, float m33) {
        m_[0][0] = m00; m_[0][1] = m01; m_[0][2] = m02; m_[0][3] = m03;
        m_[1][0] = m10; m_[1][1] = m11; m_[1][2] = m12; m_[1][3] = m13;
        m_[2][0] = m20; m_[2][1] = m21; m_[2][2] = m22; m_[2][3] = m23;
        m_[3][0] = m30; m_[3][1] = m31; m_[3][2] = m32; m_[3][3] = m33;
    }

    // 行列乗算
    Matrix4x4 operator*(const Matrix4x4& rhs) const {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m_[i][j] = m_[i][0] * rhs.m_[0][j] +
                                  m_[i][1] * rhs.m_[1][j] +
                                  m_[i][2] * rhs.m_[2][j] +
                                  m_[i][3] * rhs.m_[3][j];
            }
        }
        return result;
    }

    Matrix4x4& operator*=(const Matrix4x4& rhs) {
        *this = *this * rhs;
        return *this;
    }

    // ベクトル変換（点: w=1として変換、パースペクティブ除算あり）
    Vector3 TransformPoint(const Vector3& point) const {
        float x = point.GetX() * m_[0][0] + point.GetY() * m_[1][0] + point.GetZ() * m_[2][0] + m_[3][0];
        float y = point.GetX() * m_[0][1] + point.GetY() * m_[1][1] + point.GetZ() * m_[2][1] + m_[3][1];
        float z = point.GetX() * m_[0][2] + point.GetY() * m_[1][2] + point.GetZ() * m_[2][2] + m_[3][2];
        float w = point.GetX() * m_[0][3] + point.GetY() * m_[1][3] + point.GetZ() * m_[2][3] + m_[3][3];
        if (std::abs(w) > 1e-6f) {
            float invW = 1.0f / w;
            return Vector3(x * invW, y * invW, z * invW);
        }
        return Vector3(x, y, z);
    }

    // 方向変換（w=0として変換、平行移動なし）
    Vector3 TransformDirection(const Vector3& dir) const {
        return Vector3(
            dir.GetX() * m_[0][0] + dir.GetY() * m_[1][0] + dir.GetZ() * m_[2][0],
            dir.GetX() * m_[0][1] + dir.GetY() * m_[1][1] + dir.GetZ() * m_[2][1],
            dir.GetX() * m_[0][2] + dir.GetY() * m_[1][2] + dir.GetZ() * m_[2][2]
        );
    }

    // Vector4変換
    Vector4 TransformVector4(const Vector4& vec) const {
        return Vector4(
            vec.GetX() * m_[0][0] + vec.GetY() * m_[1][0] + vec.GetZ() * m_[2][0] + vec.GetW() * m_[3][0],
            vec.GetX() * m_[0][1] + vec.GetY() * m_[1][1] + vec.GetZ() * m_[2][1] + vec.GetW() * m_[3][1],
            vec.GetX() * m_[0][2] + vec.GetY() * m_[1][2] + vec.GetZ() * m_[2][2] + vec.GetW() * m_[3][2],
            vec.GetX() * m_[0][3] + vec.GetY() * m_[1][3] + vec.GetZ() * m_[2][3] + vec.GetW() * m_[3][3]
        );
    }

    // 転置
    Matrix4x4 Transpose() const {
        return Matrix4x4(
            m_[0][0], m_[1][0], m_[2][0], m_[3][0],
            m_[0][1], m_[1][1], m_[2][1], m_[3][1],
            m_[0][2], m_[1][2], m_[2][2], m_[3][2],
            m_[0][3], m_[1][3], m_[2][3], m_[3][3]
        );
    }

    // 行列式
    float Determinant() const {
        float a0 = m_[0][0] * m_[1][1] - m_[0][1] * m_[1][0];
        float a1 = m_[0][0] * m_[1][2] - m_[0][2] * m_[1][0];
        float a2 = m_[0][0] * m_[1][3] - m_[0][3] * m_[1][0];
        float a3 = m_[0][1] * m_[1][2] - m_[0][2] * m_[1][1];
        float a4 = m_[0][1] * m_[1][3] - m_[0][3] * m_[1][1];
        float a5 = m_[0][2] * m_[1][3] - m_[0][3] * m_[1][2];
        float b0 = m_[2][0] * m_[3][1] - m_[2][1] * m_[3][0];
        float b1 = m_[2][0] * m_[3][2] - m_[2][2] * m_[3][0];
        float b2 = m_[2][0] * m_[3][3] - m_[2][3] * m_[3][0];
        float b3 = m_[2][1] * m_[3][2] - m_[2][2] * m_[3][1];
        float b4 = m_[2][1] * m_[3][3] - m_[2][3] * m_[3][1];
        float b5 = m_[2][2] * m_[3][3] - m_[2][3] * m_[3][2];
        return a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
    }

    // 逆行列
    Matrix4x4 Inverse() const {
        float a0 = m_[0][0] * m_[1][1] - m_[0][1] * m_[1][0];
        float a1 = m_[0][0] * m_[1][2] - m_[0][2] * m_[1][0];
        float a2 = m_[0][0] * m_[1][3] - m_[0][3] * m_[1][0];
        float a3 = m_[0][1] * m_[1][2] - m_[0][2] * m_[1][1];
        float a4 = m_[0][1] * m_[1][3] - m_[0][3] * m_[1][1];
        float a5 = m_[0][2] * m_[1][3] - m_[0][3] * m_[1][2];
        float b0 = m_[2][0] * m_[3][1] - m_[2][1] * m_[3][0];
        float b1 = m_[2][0] * m_[3][2] - m_[2][2] * m_[3][0];
        float b2 = m_[2][0] * m_[3][3] - m_[2][3] * m_[3][0];
        float b3 = m_[2][1] * m_[3][2] - m_[2][2] * m_[3][1];
        float b4 = m_[2][1] * m_[3][3] - m_[2][3] * m_[3][1];
        float b5 = m_[2][2] * m_[3][3] - m_[2][3] * m_[3][2];

        float det = a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
        if (std::abs(det) < 1e-10f) {
            return Identity(); // 特異行列の場合は単位行列を返す
        }
        float invDet = 1.0f / det;

        Matrix4x4 result;
        result.m_[0][0] = (+m_[1][1] * b5 - m_[1][2] * b4 + m_[1][3] * b3) * invDet;
        result.m_[0][1] = (-m_[0][1] * b5 + m_[0][2] * b4 - m_[0][3] * b3) * invDet;
        result.m_[0][2] = (+m_[3][1] * a5 - m_[3][2] * a4 + m_[3][3] * a3) * invDet;
        result.m_[0][3] = (-m_[2][1] * a5 + m_[2][2] * a4 - m_[2][3] * a3) * invDet;
        result.m_[1][0] = (-m_[1][0] * b5 + m_[1][2] * b2 - m_[1][3] * b1) * invDet;
        result.m_[1][1] = (+m_[0][0] * b5 - m_[0][2] * b2 + m_[0][3] * b1) * invDet;
        result.m_[1][2] = (-m_[3][0] * a5 + m_[3][2] * a2 - m_[3][3] * a1) * invDet;
        result.m_[1][3] = (+m_[2][0] * a5 - m_[2][2] * a2 + m_[2][3] * a1) * invDet;
        result.m_[2][0] = (+m_[1][0] * b4 - m_[1][1] * b2 + m_[1][3] * b0) * invDet;
        result.m_[2][1] = (-m_[0][0] * b4 + m_[0][1] * b2 - m_[0][3] * b0) * invDet;
        result.m_[2][2] = (+m_[3][0] * a4 - m_[3][1] * a2 + m_[3][3] * a0) * invDet;
        result.m_[2][3] = (-m_[2][0] * a4 + m_[2][1] * a2 - m_[2][3] * a0) * invDet;
        result.m_[3][0] = (-m_[1][0] * b3 + m_[1][1] * b1 - m_[1][2] * b0) * invDet;
        result.m_[3][1] = (+m_[0][0] * b3 - m_[0][1] * b1 + m_[0][2] * b0) * invDet;
        result.m_[3][2] = (-m_[3][0] * a3 + m_[3][1] * a1 - m_[3][2] * a0) * invDet;
        result.m_[3][3] = (+m_[2][0] * a3 - m_[2][1] * a1 + m_[2][2] * a0) * invDet;
        return result;
    }

    // 要素アクセス
    float GetElement(uint32 row, uint32 col) const { return m_[row][col]; }
    void SetElement(uint32 row, uint32 col, float value) { m_[row][col] = value; }

    // float配列への変換（ImGuizmo用）
    void ToFloatArray(float* out) const {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out[i * 4 + j] = m_[i][j];
            }
        }
    }

    // float配列からの変換
    static Matrix4x4 FromFloatArray(const float* data) {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m_[i][j] = data[i * 4 + j];
            }
        }
        return result;
    }

    // 静的メンバ - 生成関数
    static Matrix4x4 Identity() { return Matrix4x4(); }

    static Matrix4x4 Translation(float x, float y, float z) {
        Matrix4x4 result;
        result.m_[3][0] = x;
        result.m_[3][1] = y;
        result.m_[3][2] = z;
        return result;
    }

    static Matrix4x4 Translation(const Vector3& pos) {
        return Translation(pos.GetX(), pos.GetY(), pos.GetZ());
    }

    static Matrix4x4 Scaling(float x, float y, float z) {
        Matrix4x4 result;
        result.m_[0][0] = x;
        result.m_[1][1] = y;
        result.m_[2][2] = z;
        return result;
    }

    static Matrix4x4 Scaling(const Vector3& scale) {
        return Scaling(scale.GetX(), scale.GetY(), scale.GetZ());
    }

    static Matrix4x4 Scaling(float uniformScale) {
        return Scaling(uniformScale, uniformScale, uniformScale);
    }

    static Matrix4x4 Scale(const Vector3& scale) {
        return Scaling(scale);
    }

    static Matrix4x4 CreateScale(const Vector3& scale) {
        return Scaling(scale);
    }

    static Matrix4x4 CreateTranslation(const Vector3& pos) {
        return Translation(pos);
    }

    static Matrix4x4 CreateFromQuaternion(const Quaternion& q);

    // X軸回転
    static Matrix4x4 RotationX(float radians) {
        float c = std::cos(radians);
        float s = std::sin(radians);
        Matrix4x4 result;
        result.m_[1][1] = c;
        result.m_[1][2] = s;
        result.m_[2][1] = -s;
        result.m_[2][2] = c;
        return result;
    }

    // Y軸回転
    static Matrix4x4 RotationY(float radians) {
        float c = std::cos(radians);
        float s = std::sin(radians);
        Matrix4x4 result;
        result.m_[0][0] = c;
        result.m_[0][2] = -s;
        result.m_[2][0] = s;
        result.m_[2][2] = c;
        return result;
    }

    // Z軸回転
    static Matrix4x4 RotationZ(float radians) {
        float c = std::cos(radians);
        float s = std::sin(radians);
        Matrix4x4 result;
        result.m_[0][0] = c;
        result.m_[0][1] = s;
        result.m_[1][0] = -s;
        result.m_[1][1] = c;
        return result;
    }

    // 任意軸回転
    static Matrix4x4 RotationAxis(const Vector3& axis, float radians) {
        Vector3 n = axis.Normalize();
        float c = std::cos(radians);
        float s = std::sin(radians);
        float t = 1.0f - c;
        float x = n.GetX(), y = n.GetY(), z = n.GetZ();

        return Matrix4x4(
            t * x * x + c,      t * x * y + s * z,  t * x * z - s * y,  0.0f,
            t * x * y - s * z,  t * y * y + c,      t * y * z + s * x,  0.0f,
            t * x * z + s * y,  t * y * z - s * x,  t * z * z + c,      0.0f,
            0.0f,               0.0f,               0.0f,               1.0f
        );
    }

    // カメラ行列（左手系）
    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) {
        Vector3 zAxis = (target - eye).Normalize();
        Vector3 xAxis = up.Cross(zAxis).Normalize();
        Vector3 yAxis = zAxis.Cross(xAxis);

        return Matrix4x4(
            xAxis.GetX(), yAxis.GetX(), zAxis.GetX(), 0.0f,
            xAxis.GetY(), yAxis.GetY(), zAxis.GetY(), 0.0f,
            xAxis.GetZ(), yAxis.GetZ(), zAxis.GetZ(), 0.0f,
            -xAxis.Dot(eye), -yAxis.Dot(eye), -zAxis.Dot(eye), 1.0f
        );
    }

    static Matrix4x4 LookToLH(const Vector3& eye, const Vector3& direction, const Vector3& up) {
        Vector3 zAxis = direction.Normalize();
        Vector3 xAxis = up.Cross(zAxis).Normalize();
        Vector3 yAxis = zAxis.Cross(xAxis);

        return Matrix4x4(
            xAxis.GetX(), yAxis.GetX(), zAxis.GetX(), 0.0f,
            xAxis.GetY(), yAxis.GetY(), zAxis.GetY(), 0.0f,
            xAxis.GetZ(), yAxis.GetZ(), zAxis.GetZ(), 0.0f,
            -xAxis.Dot(eye), -yAxis.Dot(eye), -zAxis.Dot(eye), 1.0f
        );
    }

    // 透視投影行列（左手系）
    static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) {
        float yScale = 1.0f / std::tan(fovY * 0.5f);
        float xScale = yScale / aspect;
        float zRange = farZ / (farZ - nearZ);

        Matrix4x4 result;
        std::memset(result.m_, 0, sizeof(result.m_));
        result.m_[0][0] = xScale;
        result.m_[1][1] = yScale;
        result.m_[2][2] = zRange;
        result.m_[2][3] = 1.0f;
        result.m_[3][2] = -nearZ * zRange;
        return result;
    }

    // 正射影行列（左手系）
    static Matrix4x4 OrthographicLH(float width, float height, float nearZ, float farZ) {
        float zRange = 1.0f / (farZ - nearZ);

        Matrix4x4 result;
        std::memset(result.m_, 0, sizeof(result.m_));
        result.m_[0][0] = 2.0f / width;
        result.m_[1][1] = 2.0f / height;
        result.m_[2][2] = zRange;
        result.m_[3][2] = -nearZ * zRange;
        result.m_[3][3] = 1.0f;
        return result;
    }

    // 線形補間
    static Matrix4x4 Lerp(const Matrix4x4& a, const Matrix4x4& b, float t) {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m_[i][j] = a.m_[i][j] + (b.m_[i][j] - a.m_[i][j]) * t;
            }
        }
        return result;
    }

private:
    float m_[4][4];
};

} // namespace UnoEngine
