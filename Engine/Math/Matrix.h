#pragma once

#include "MathCommon.h"
#include "Vector.h"

namespace UnoEngine {

// 4x4行列
class Matrix4x4 {
public:
    Matrix4x4() : mat_(DirectX::XMMatrixIdentity()) {}
    explicit Matrix4x4(DirectX::FXMMATRIX m) : mat_(m) {}

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
