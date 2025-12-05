#include "pch.h"
#include "CameraComponent.h"
#include "GameObject.h"
#include "Transform.h"
#include <algorithm>

namespace UnoEngine {

void CameraComponent::Awake() {
    // GameObjectのTransformからカメラのTransformを即座に同期
    // これを最初に行わないと、SetActiveCamera()時にカメラの方向ベクトルがゼロになる
    UpdateCameraTransform();

    // 初期投影設定
    UpdateProjectionMatrix();

}

void CameraComponent::Start() {
    // TransformからカメラのPosition/Rotationを初期化
    UpdateCameraTransform();
}

void CameraComponent::OnUpdate(float deltaTime) {
    (void)deltaTime;

    // GameObjectのTransformからカメラのTransformを同期
    UpdateCameraTransform();

    // 投影行列の更新が必要な場合
    if (updateProjection_) {
        UpdateProjectionMatrix();
        updateProjection_ = false;
    }
}

void CameraComponent::OnDestroy() {
    // 特に必要な処理なし
}

void CameraComponent::SetPerspective(float fovY, float aspect, float nearZ, float farZ) {
    fovY_ = fovY;
    aspect_ = aspect;
    nearZ_ = nearZ;
    farZ_ = farZ;
    isOrthographic_ = false;
    updateProjection_ = true;
}

void CameraComponent::SetOrthographic(float width, float height, float nearZ, float farZ) {
    orthoWidth_ = width;
    orthoHeight_ = height;
    nearZ_ = nearZ;
    farZ_ = farZ;
    isOrthographic_ = true;
    updateProjection_ = true;
}

const Matrix4x4& CameraComponent::GetViewMatrix() {
    return camera_.GetViewMatrix();
}

Matrix4x4 CameraComponent::GetViewProjectionMatrix() {
    return camera_.GetViewProjectionMatrix();
}

void CameraComponent::UpdateCameraTransform() {
    if (!gameObject_) return;

    auto& transform = gameObject_->GetTransform();

    // TransformのGetPosition()を使用（正しくワールド位置を取得）
    Vector3 worldPos = transform.GetPosition();
    camera_.SetPosition(worldPos);

    // ワールド回転を使用（親の回転も考慮）
    camera_.SetRotation(transform.GetRotation());
}

void CameraComponent::UpdateProjectionMatrix() {
    if (isOrthographic_) {
        camera_.SetOrthographic(orthoWidth_, orthoHeight_, nearZ_, farZ_);
    } else {
        camera_.SetPerspective(fovY_, aspect_, nearZ_, farZ_);
    }
}

void CameraComponent::GetFrustumCorners(Vector3 outNearCorners[4], Vector3 outFarCorners[4]) const {
    // カメラの位置と方向
    Vector3 pos = camera_.GetPosition();
    Vector3 forward = camera_.GetForward();
    Vector3 right = camera_.GetRight();
    Vector3 up = camera_.GetUp();

    if (isOrthographic_) {
        // Orthographic
        float halfW = orthoWidth_ * 0.5f;
        float halfH = orthoHeight_ * 0.5f;

        Vector3 nearCenter = pos + forward * nearZ_;
        Vector3 farCenter = pos + forward * farZ_;

        // Near plane corners
        outNearCorners[0] = nearCenter - right * halfW - up * halfH;  // bottom-left
        outNearCorners[1] = nearCenter + right * halfW - up * halfH;  // bottom-right
        outNearCorners[2] = nearCenter + right * halfW + up * halfH;  // top-right
        outNearCorners[3] = nearCenter - right * halfW + up * halfH;  // top-left

        // Far plane corners
        outFarCorners[0] = farCenter - right * halfW - up * halfH;
        outFarCorners[1] = farCenter + right * halfW - up * halfH;
        outFarCorners[2] = farCenter + right * halfW + up * halfH;
        outFarCorners[3] = farCenter - right * halfW + up * halfH;
    } else {
        // Perspective
        float tanHalfFov = std::tan(fovY_ * 0.5f);

        float nearH = nearZ_ * tanHalfFov;
        float nearW = nearH * aspect_;
        float farH = farZ_ * tanHalfFov;
        float farW = farH * aspect_;

        Vector3 nearCenter = pos + forward * nearZ_;
        Vector3 farCenter = pos + forward * farZ_;

        // Near plane corners
        outNearCorners[0] = nearCenter - right * nearW - up * nearH;  // bottom-left
        outNearCorners[1] = nearCenter + right * nearW - up * nearH;  // bottom-right
        outNearCorners[2] = nearCenter + right * nearW + up * nearH;  // top-right
        outNearCorners[3] = nearCenter - right * nearW + up * nearH;  // top-left

        // Far plane corners
        outFarCorners[0] = farCenter - right * farW - up * farH;
        outFarCorners[1] = farCenter + right * farW - up * farH;
        outFarCorners[2] = farCenter + right * farW + up * farH;
        outFarCorners[3] = farCenter - right * farW + up * farH;
    }
}

void CameraComponent::SetPostProcessEffect(PostProcessType effect) {
    postProcessEffects_.clear();
    if (effect != PostProcessType::None) {
        postProcessEffects_.push_back(effect);
    }
}

void CameraComponent::AddPostProcessEffect(PostProcessType effect) {
    if (effect == PostProcessType::None || effect == PostProcessType::Count) return;
    if (HasPostProcessEffect(effect)) return;
    postProcessEffects_.push_back(effect);
}

void CameraComponent::RemovePostProcessEffect(PostProcessType effect) {
    auto it = std::find(postProcessEffects_.begin(), postProcessEffects_.end(), effect);
    if (it != postProcessEffects_.end()) {
        postProcessEffects_.erase(it);
    }
}

bool CameraComponent::HasPostProcessEffect(PostProcessType effect) const {
    return std::find(postProcessEffects_.begin(), postProcessEffects_.end(), effect) != postProcessEffects_.end();
}

} // namespace UnoEngine
