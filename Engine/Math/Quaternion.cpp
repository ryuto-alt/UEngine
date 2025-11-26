#include "Quaternion.h"
#include "Matrix.h"

namespace UnoEngine {

Quaternion Quaternion::FromRotationMatrix(const Matrix4x4& mat) {
    return Quaternion(DirectX::XMQuaternionRotationMatrix(mat.GetXMMatrix()));
}

Matrix4x4 Quaternion::ToMatrix() const {
    return Matrix4x4(DirectX::XMMatrixRotationQuaternion(quat_));
}

Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up) {
    Matrix4x4 lookMat = Matrix4x4::LookToLH(Vector3::Zero(), forward, up);
    return FromRotationMatrix(lookMat);
}

// Matrix4x4::CreateFromQuaternion の実装
Matrix4x4 Matrix4x4::CreateFromQuaternion(const Quaternion& q) {
    return q.ToMatrix();
}

} // namespace UnoEngine
