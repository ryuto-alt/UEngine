#pragma once

#include "../Math/Math.h"

namespace UnoEngine {

// カメラ投影タイプ
enum class ProjectionType {
    Perspective,
    Orthographic
};

// カメラクラス
class Camera {
public:
    Camera();
    ~Camera() = default;

    // ビュー行列更新
    void UpdateViewMatrix();

    // 投影行列設定
    void SetPerspective(float fovY, float aspect, float nearZ, float farZ);
    void SetOrthographic(float width, float height, float nearZ, float farZ);

    // 位置・回転設定
    void SetPosition(const Vector3& pos) { position_ = pos; dirtyView_ = true; }
    void SetRotation(const Quaternion& rot) { rotation_ = rot; dirtyView_ = true; }
    void SetTarget(const Vector3& target);

    // 移動
    void Translate(const Vector3& delta) { position_ += delta; dirtyView_ = true; }
    void Rotate(const Quaternion& delta) { rotation_ *= delta; dirtyView_ = true; }

    // 方向ベクトル取得
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;

    // アクセサ
    const Vector3& GetPosition() const { return position_; }
    const Quaternion& GetRotation() const { return rotation_; }
    const Matrix4x4& GetViewMatrix();
    const Matrix4x4& GetProjectionMatrix() const { return projection_; }
    Matrix4x4 GetViewProjectionMatrix();

    float GetNearClip() const { return nearZ_; }
    float GetFarClip() const { return farZ_; }
    float GetAspectRatio() const { return aspect_; }
    float GetFieldOfView() const { return fovY_; }

private:
    // トランスフォーム
    Vector3 position_;
    Quaternion rotation_;

    // 行列
    Matrix4x4 view_;
    Matrix4x4 projection_;

    // 投影パラメータ
    ProjectionType projectionType_;
    float fovY_;
    float aspect_;
    float width_;
    float height_;
    float nearZ_;
    float farZ_;

    // ダーティフラグ
    bool dirtyView_;
};

} // namespace UnoEngine
