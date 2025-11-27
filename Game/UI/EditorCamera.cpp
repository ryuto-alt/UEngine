#include "EditorCamera.h"
#include "../../Engine/Math/Quaternion.h"
#include <imgui.h>
#include <cmath>

namespace UnoEngine {

void EditorCamera::Update(float deltaTime) {
    if (!camera_) return;

    // Viewport内でのみ操作を受け付ける
    if (!viewportHovered_) {
        isControlling_ = false;
        rightMousePressed_ = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // 右クリック状態の更新
    bool rightMouseDown = io.MouseDown[1];  // ImGuiMouseButton_Right

    if (rightMouseDown && !rightMousePressed_) {
        // 右クリック開始
        rightMousePressed_ = true;
        lastMousePosX_ = io.MousePos.x;
        lastMousePosY_ = io.MousePos.y;
        isControlling_ = true;
    } else if (!rightMouseDown && rightMousePressed_) {
        // 右クリック終了
        rightMousePressed_ = false;
        isControlling_ = false;
    }

    // カメラ操作
    if (rightMousePressed_) {
        HandleFreeCameraMovement(deltaTime);
        HandleMouseRotation(deltaTime);
    }

    // スクロールズーム（右クリック不要）
    HandleScrollZoom(deltaTime);

    // マウス位置を更新
    lastMousePosX_ = io.MousePos.x;
    lastMousePosY_ = io.MousePos.y;
}

void EditorCamera::HandleFreeCameraMovement(float deltaTime) {
    if (!camera_) return;

    ImGuiIO& io = ImGui::GetIO();
    
    // ImGuiがキーボードを使用していない時のみ
    if (io.WantCaptureKeyboard) return;

    Vector3 movement(0.0f, 0.0f, 0.0f);
    float speed = moveSpeed_;

    // Shiftで加速
    if (io.KeyShift) {
        speed *= 2.0f;
    }

    // WASDで移動
    if (ImGui::IsKeyDown(ImGuiKey_W)) {
        movement = movement + camera_->GetForward();
    }
    if (ImGui::IsKeyDown(ImGuiKey_S)) {
        movement = movement - camera_->GetForward();
    }
    if (ImGui::IsKeyDown(ImGuiKey_A)) {
        movement = movement - camera_->GetRight();
    }
    if (ImGui::IsKeyDown(ImGuiKey_D)) {
        movement = movement + camera_->GetRight();
    }

    // Q/Eで上下移動
    if (ImGui::IsKeyDown(ImGuiKey_Q)) {
        movement = movement - camera_->GetUp();
    }
    if (ImGui::IsKeyDown(ImGuiKey_E)) {
        movement = movement + camera_->GetUp();
    }

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
        // Yaw（左右回転）- Y軸周り
        float yaw = -deltaX * rotateSpeed_ * deltaTime;
        // Pitch（上下回転）- X軸周り
        float pitch = -deltaY * rotateSpeed_ * deltaTime;

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

} // namespace UnoEngine
