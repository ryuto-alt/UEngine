#include "EditorCamera.h"
#include "../../Engine/Math/Quaternion.h"
#include <imgui.h>
#include <cmath>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace UnoEngine {

void EditorCamera::Update(float deltaTime) {
    if (!camera_) return;

    ImGuiIO& io = ImGui::GetIO();
    bool rightMouseDown = io.MouseDown[1];

    // Viewport内でのみ操作を受け付ける
    if (!viewportHovered_) {
        isControlling_ = false;
        if (rightMousePressed_) {
            rightMousePressed_ = false;
            while (ShowCursor(TRUE) < 0);
        }
        return;
    }

    if (rightMouseDown && !rightMousePressed_) {
        // 右クリック開始
        rightMousePressed_ = true;
        isControlling_ = true;
        while (ShowCursor(FALSE) >= 0);

        // マウス位置を記憶
        GetCursorPos(&lockMousePos_);

        // 現在のカメラ回転からyaw/pitchを取得
        Vector3 forward = camera_->GetForward();
        yaw_ = std::atan2(forward.GetX(), forward.GetZ());
        pitch_ = std::asin(-forward.GetY());
    } else if (!rightMouseDown && rightMousePressed_) {
        // 右クリック終了
        rightMousePressed_ = false;
        isControlling_ = false;
        while (ShowCursor(TRUE) < 0);
    }

    // カメラ操作
    if (rightMousePressed_) {
        POINT currentPos;
        GetCursorPos(&currentPos);

        float deltaX = static_cast<float>(currentPos.x - lockMousePos_.x);
        float deltaY = static_cast<float>(currentPos.y - lockMousePos_.y);

        SetCursorPos(lockMousePos_.x, lockMousePos_.y);

        // 角度を累積（Pitchは上下89度に制限）
        yaw_ += deltaX * rotateSpeed_ * deltaTime;
        pitch_ -= deltaY * rotateSpeed_ * deltaTime;
        const float maxPitch = 1.55f; // 約89度
        if (pitch_ > maxPitch) pitch_ = maxPitch;
        if (pitch_ < -maxPitch) pitch_ = -maxPitch;

        // Yaw→Pitchの順で回転（ロールなし）
        Quaternion yawRot = Quaternion::RotationAxis(Vector3::UnitY(), yaw_);
        Quaternion pitchRot = Quaternion::RotationAxis(Vector3::UnitX(), pitch_);
        camera_->SetRotation(yawRot * pitchRot);

        HandleFreeCameraMovement(deltaTime);
    }

    // スクロールズーム（右クリック不要）
    HandleScrollZoom(deltaTime);
}

void EditorCamera::HandleFreeCameraMovement(float deltaTime) {
    if (!camera_) return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    float speed = moveSpeed_;
    if (io.KeyShift) speed *= 2.0f;

    // 移動方向を計算（Y軸固定で水平移動）
    Vector3 forward = camera_->GetForward();
    Vector3 right = camera_->GetRight();

    // 水平成分のみ使用
    forward = Vector3(forward.GetX(), 0.0f, forward.GetZ());
    right = Vector3(right.GetX(), 0.0f, right.GetZ());

    if (forward.Length() > 0.001f) forward = forward.Normalize();
    if (right.Length() > 0.001f) right = right.Normalize();

    Vector3 movement(0.0f, 0.0f, 0.0f);

    // WASD移動
    if (ImGui::IsKeyDown(ImGuiKey_W)) movement = movement + forward;
    if (ImGui::IsKeyDown(ImGuiKey_S)) movement = movement - forward;
    if (ImGui::IsKeyDown(ImGuiKey_A)) movement = movement - right;
    if (ImGui::IsKeyDown(ImGuiKey_D)) movement = movement + right;

    // Space/Ctrlで上下移動
    if (ImGui::IsKeyDown(ImGuiKey_Space)) movement = movement + Vector3::UnitY();
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) movement = movement - Vector3::UnitY();

    // 移動を適用
    if (movement.Length() > 0.001f) {
        movement = movement.Normalize() * speed * deltaTime;
        camera_->Translate(movement);
    }
}

void EditorCamera::HandleMouseRotation(float deltaTime) {
    if (!camera_) return;

    ImGuiIO& io = ImGui::GetIO();

    float deltaX = io.MousePos.x - lastMousePosX_;
    float deltaY = io.MousePos.y - lastMousePosY_;

    if (std::abs(deltaX) > 0.001f || std::abs(deltaY) > 0.001f) {
        // Yaw（左右回転）- マウス右で右向き
        float yaw = deltaX * rotateSpeed_ * deltaTime;
        // Pitch（上下回転）- マウス上で上向き
        float pitch = deltaY * rotateSpeed_ * deltaTime;

        // Quaternionで回転を適用
        Quaternion yawRot = Quaternion::RotationAxis(Vector3::UnitY(), yaw);
        Quaternion pitchRot = Quaternion::RotationAxis(camera_->GetRight(), pitch);

        camera_->Rotate(yawRot * pitchRot);
    }
}

void EditorCamera::HandleScrollZoom(float deltaTime) {
    if (!camera_) return;

    ImGuiIO& io = ImGui::GetIO();

    float scroll = io.MouseWheel;
    if (std::abs(scroll) > 0.001f) {
        Vector3 forward = camera_->GetForward();
        camera_->Translate(forward * scroll * scrollSpeed_);
    }
}

void EditorCamera::FocusOn(const Vector3& targetPosition, float distance) {
    if (!camera_) return;

    // 現在のカメラ方向を維持したまま、ターゲットから一定距離に配置
    Vector3 forward = camera_->GetForward();
    Vector3 newCameraPos = targetPosition - forward * distance;
    camera_->SetPosition(newCameraPos);
}

} // namespace UnoEngine
