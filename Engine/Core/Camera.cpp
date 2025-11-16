#include "Camera.h"

namespace UnoEngine {

Camera::Camera()
    : position_(0.0f, 0.0f, -5.0f)
    , rotation_(Quaternion::Identity())
    , view_(Matrix4x4::Identity())
    , projection_(Matrix4x4::Identity())
    , projectionType_(ProjectionType::Perspective)
    , fovY_(Math::ToRadians(60.0f))
    , aspect_(16.0f / 9.0f)
    , width_(1280.0f)
    , height_(720.0f)
    , nearZ_(0.1f)
    , farZ_(1000.0f)
    , dirtyView_(true) {

    SetPerspective(fovY_, aspect_, nearZ_, farZ_);
}

void Camera::UpdateViewMatrix() {
    if (!dirtyView_) return;

    // カメラの方向ベクトルを取得
    Vector3 forward = GetForward();
    Vector3 up = GetUp();

    // LookTo行列を作成
    view_ = Matrix4x4::LookToLH(position_, forward, up);
    dirtyView_ = false;
}

void Camera::SetPerspective(float fovY, float aspect, float nearZ, float farZ) {
    projectionType_ = ProjectionType::Perspective;
    fovY_ = fovY;
    aspect_ = aspect;
    nearZ_ = nearZ;
    farZ_ = farZ;

    projection_ = Matrix4x4::PerspectiveFovLH(fovY_, aspect_, nearZ_, farZ_);
}

void Camera::SetOrthographic(float width, float height, float nearZ, float farZ) {
    projectionType_ = ProjectionType::Orthographic;
    width_ = width;
    height_ = height;
    nearZ_ = nearZ;
    farZ_ = farZ;

    projection_ = Matrix4x4::OrthographicLH(width_, height_, nearZ_, farZ_);
}

void Camera::SetTarget(const Vector3& target) {
    Vector3 direction = (target - position_).Normalize();
    Vector3 up = Vector3::UnitY();

    // 前方向からクォータニオンを計算
    // 簡易実装: LookAt風の回転を設定
    Vector3 forward = Vector3::UnitZ();
    float dot = forward.Dot(direction);

    if (Math::Abs(dot - (-1.0f)) < Math::EPSILON) {
        rotation_ = Quaternion::RotationAxis(Vector3::UnitY(), Math::PI);
    } else if (Math::Abs(dot - 1.0f) < Math::EPSILON) {
        rotation_ = Quaternion::Identity();
    } else {
        float angle = std::acos(dot);
        Vector3 axis = forward.Cross(direction).Normalize();
        rotation_ = Quaternion::RotationAxis(axis, angle);
    }

    dirtyView_ = true;
}

Vector3 Camera::GetForward() const {
    return rotation_.RotateVector(Vector3::UnitZ());
}

Vector3 Camera::GetRight() const {
    return rotation_.RotateVector(Vector3::UnitX());
}

Vector3 Camera::GetUp() const {
    return rotation_.RotateVector(Vector3::UnitY());
}

const Matrix4x4& Camera::GetViewMatrix() {
    UpdateViewMatrix();
    return view_;
}

Matrix4x4 Camera::GetViewProjectionMatrix() {
    UpdateViewMatrix();
    return view_ * projection_;
}

} // namespace UnoEngine
