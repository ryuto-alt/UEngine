#pragma once

#include "MathCommon.h"
#include "Vector.h"
#include "../Core/Types.h"

namespace UnoEngine {

// 4x4行列
class Matrix4x4 {
public:
    Matrix4x4() : mat_(DirectX::XMMatrixIdentity()) {}
    explicit Matrix4x4(DirectX::FXMMATRIX m) : mat_(m) {}

    // 16要素コンストラクタ (行優先)
    Matrix4x4(float m00, float m01, float m02, float m03,
              float m10, float m11, float m12, float m13,
              float m20, float m21, float m22, float m23,
              float m30, float m31, float m32, float m33) {
        mat_ = DirectX::XMMatrixSet(
            m00, m01, m02, m03,
            m10, m11, m12, m13,
            m20, m21, m22, m23,
            m30, m31, m32, m33
        );
    }

    // 行列演算
    Matrix4x4 operator*(const Matrix4x4& rhs) const { return Matrix4x4(DirectX::XMMatrixMultiply(mat_, rhs.mat_)); }
    Matrix4x4& operator*=(const Matrix4x4& rhs) { mat_ = DirectX::XMMatrixMultiply(mat_, rhs.mat_); return *this; }

    // ベクトル変換
    Vector3 TransformPoint(const Vector3& point) const {
        DirectX::XMVECTOR v = DirectX::XMVector3TransformCoord(point.GetXMVector(), mat_);
        return Vector3(v);
    }

    Vector3 TransformDirection(const Vector3& dir) const {
        DirectX::XMVECTOR v = DirectX::XMVector3TransformNormal(dir.GetXMVector(), mat_);
        return Vector3(v);
    }

    Vector4 TransformVector4(const Vector4& vec) const {
        DirectX::XMVECTOR v = DirectX::XMVector4Transform(vec.GetXMVector(), mat_);
        return Vector4(v);
    }

    // 行列操作
    Matrix4x4 Transpose() const { return Matrix4x4(DirectX::XMMatrixTranspose(mat_)); }
    Matrix4x4 Inverse() const {
        DirectX::XMVECTOR det;
        return Matrix4x4(DirectX::XMMatrixInverse(&det, mat_));
    }

    float Determinant() const { return DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(mat_)); }

    // Element access
    float GetElement(uint32 row, uint32 col) const {
        DirectX::XMFLOAT4X4 m;
        DirectX::XMStoreFloat4x4(&m, mat_);
        return m.m[row][col];
    }

    // float配列への変換（ImGuizmo用）
    void ToFloatArray(float* out) const {
        DirectX::XMFLOAT4X4 m;
        DirectX::XMStoreFloat4x4(&m, mat_);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out[i * 4 + j] = m.m[i][j];
            }
        }
    }

    // float配列からの変換
    static Matrix4x4 FromFloatArray(const float* data) {
        DirectX::XMFLOAT4X4 m;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m.m[i][j] = data[i * 4 + j];
            }
        }
        return Matrix4x4(DirectX::XMLoadFloat4x4(&m));
    }

    // DirectXMathへの変換
    DirectX::XMMATRIX GetXMMatrix() const { return mat_; }

    // 静的メンバ - 生成関数
    static Matrix4x4 Identity() { return Matrix4x4(); }

    static Matrix4x4 Translation(float x, float y, float z) {
        return Matrix4x4(DirectX::XMMatrixTranslation(x, y, z));
    }

    static Matrix4x4 Translation(const Vector3& pos) {
        return Translation(pos.GetX(), pos.GetY(), pos.GetZ());
    }

    static Matrix4x4 Scaling(float x, float y, float z) {
        return Matrix4x4(DirectX::XMMatrixScaling(x, y, z));
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

    // Aliasメソッド (AnimationClip互換)
    static Matrix4x4 CreateScale(const Vector3& scale) {
        return Scaling(scale);
    }

    static Matrix4x4 CreateTranslation(const Vector3& pos) {
        return Translation(pos);
    }

    static Matrix4x4 CreateFromQuaternion(const class Quaternion& q);

    // 線形補間
    static Matrix4x4 Lerp(const Matrix4x4& a, const Matrix4x4& b, float t) {
        // 行列の要素ごとに線形補間
        DirectX::XMFLOAT4X4 ma, mb, result;
        DirectX::XMStoreFloat4x4(&ma, a.mat_);
        DirectX::XMStoreFloat4x4(&mb, b.mat_);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = ma.m[i][j] + (mb.m[i][j] - ma.m[i][j]) * t;
            }
        }
        return Matrix4x4(DirectX::XMLoadFloat4x4(&result));
    }

    static Matrix4x4 RotationX(float radians) {
        return Matrix4x4(DirectX::XMMatrixRotationX(radians));
    }

    static Matrix4x4 RotationY(float radians) {
        return Matrix4x4(DirectX::XMMatrixRotationY(radians));
    }

    static Matrix4x4 RotationZ(float radians) {
        return Matrix4x4(DirectX::XMMatrixRotationZ(radians));
    }

    static Matrix4x4 RotationAxis(const Vector3& axis, float radians) {
        return Matrix4x4(DirectX::XMMatrixRotationAxis(axis.GetXMVector(), radians));
    }

    // カメラ行列
    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) {
        return Matrix4x4(DirectX::XMMatrixLookAtLH(
            eye.GetXMVector(),
            target.GetXMVector(),
            up.GetXMVector()
        ));
    }

    static Matrix4x4 LookToLH(const Vector3& eye, const Vector3& direction, const Vector3& up) {
        return Matrix4x4(DirectX::XMMatrixLookToLH(
            eye.GetXMVector(),
            direction.GetXMVector(),
            up.GetXMVector()
        ));
    }

    // 投影行列
    static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) {
        return Matrix4x4(DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
    }

    static Matrix4x4 OrthographicLH(float width, float height, float nearZ, float farZ) {
        return Matrix4x4(DirectX::XMMatrixOrthographicLH(width, height, nearZ, farZ));
    }

private:
    DirectX::XMMATRIX mat_;
};

} // namespace UnoEngine
