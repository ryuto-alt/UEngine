#include "pch.h"
#include "CameraComponent.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "../Input/InputManager.h"
#include "../Input/Keyboard.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

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
    // フォローモードの処理
    if (viewMode_ != CameraViewMode::Free) {
        UpdateFollowCamera(deltaTime);
    } else {
        // GameObjectのTransformからカメラのTransformを同期
        UpdateCameraTransform();
    }

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

GameObject* CameraComponent::FindFollowTarget() const {
    if (!scene_ || followTargetName_.empty()) return nullptr;
    
    for (const auto& obj : scene_->GetGameObjects()) {
        if (obj->GetName() == followTargetName_) {
            return obj.get();
        }
    }
    return nullptr;
}

GameObject* CameraComponent::GetFirstPersonExcludeTarget() const {
    if (viewMode_ != CameraViewMode::FirstPerson || !hideTargetInFirstPerson_) {
        return nullptr;
    }
    return FindFollowTarget();
}

void CameraComponent::UpdateFollowCamera(float deltaTime) {
    GameObject* target = FindFollowTarget();
    if (!target) {
        UpdateCameraTransform();
        return;
    }

    Vector3 targetPos = target->GetTransform().GetPosition();
    
    Vector3 desiredPos;
    Quaternion desiredRot;

    if (viewMode_ == CameraViewMode::FirstPerson) {
        // 一人称視点: マウスで視点回転のみ（移動はLuaスクリプトで管理）

        if (isPlaying_ && mouseLocked_ && scene_) {
            if (auto* input = scene_->GetInputManager()) {
                POINT currentPos;
                GetCursorPos(&currentPos);

                float deltaX = static_cast<float>(currentPos.x - mouseLockX_);
                float deltaY = static_cast<float>(currentPos.y - mouseLockY_);

                if (deltaX != 0.0f || deltaY != 0.0f) {
                    SetCursorPos(mouseLockX_, mouseLockY_);
                }

                cameraYaw_ += deltaX * mouseSensitivity_ * 0.01f;
                cameraPitch_ += deltaY * mouseSensitivity_ * 0.01f;

                constexpr float maxPitch = 1.5f;
                if (cameraPitch_ > maxPitch) cameraPitch_ = maxPitch;
                if (cameraPitch_ < -maxPitch) cameraPitch_ = -maxPitch;
            }
        }

        // ターゲット位置にオフセットを加えた位置にカメラを配置
        desiredPos = targetPos + firstPersonOffset_;

        // Y軸回転（yaw）とX軸回転（pitch）を合成
        Quaternion rotY = Quaternion::RotationAxis(Vector3::UnitY(), cameraYaw_);
        Quaternion rotX = Quaternion::RotationAxis(Vector3::UnitX(), cameraPitch_);
        desiredRot = rotY * rotX;
    } else {
        // 三人称視点: ターゲットの後ろから見下ろす
        float pitchRad = followPitch_ * 0.0174533f;
        
        Vector3 targetForward = target->GetTransform().GetForward();
        
        Vector3 offset = -targetForward * followDistance_ * std::cos(pitchRad);
        offset = offset + Vector3(0.0f, followHeight_ + followDistance_ * std::sin(pitchRad), 0.0f);
        
        desiredPos = targetPos + offset;
        
        // ターゲットを見るように回転
        Vector3 lookDir = (targetPos + Vector3(0.0f, followHeight_ * 0.5f, 0.0f)) - desiredPos;
        if (lookDir.Length() > 0.001f) {
            lookDir = lookDir.Normalize();
            float yaw = std::atan2(lookDir.GetX(), lookDir.GetZ());
            float pitch = -std::asin(lookDir.GetY());
            desiredRot = Quaternion::RotationRollPitchYaw(pitch, yaw, 0.0f);
        }
    }

    // スムーズ補間
    float t = 1.0f - std::exp(-followSmoothness_ * deltaTime);
    Vector3 currentPos = camera_.GetPosition();
    Quaternion currentRot = camera_.GetRotation();
    
    Vector3 newPos = currentPos + (desiredPos - currentPos) * t;
    Quaternion newRot = Quaternion::Slerp(currentRot, desiredRot, t);
    
    camera_.SetPosition(newPos);
    camera_.SetRotation(newRot);
    
    // GameObjectのTransformも同期（ギズモ表示用）
    if (gameObject_) {
        gameObject_->GetTransform().SetPosition(newPos);
        gameObject_->GetTransform().SetRotation(newRot);
    }
}

} // namespace UnoEngine
