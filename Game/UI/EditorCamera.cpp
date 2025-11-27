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
    } else if (!rightMouseDown && rightMousePressed_) {
        // 右クリック終了
        rightMousePressed_ = false;
        isControlling_ = false;
        while (ShowCursor(TRUE) < 0);
    }

    // カメラ操作
    if (rightMousePressed_) {
        // 現在のマウス位置を取得
        POINT currentPos;
        GetCursorPos(&currentPos);

        // 移動量を計算
        float deltaX = static_cast<float>(currentPos.x - lockMousePos_.x);
        float deltaY = static_cast<float>(currentPos.y - lockMousePos_.y);

        // マウスを固定位置に戻す
        SetCursorPos(lockMousePos_.x, lockMousePos_.y);

        // カメラ回転
        if (std::abs(deltaX) > 0.001f || std::abs(deltaY) > 0.001f) {
            float yaw = deltaX * rotateSpeed_ * deltaTime;
            float pitch = deltaY * rotateSpeed_ * deltaTime;

            Quaternion yawRot = Quaternion::RotationAxis(Vector3::UnitY(), yaw);
            Quaternion pitchRot = Quaternion::RotationAxis(camera_->GetRight(), pitch);
            camera_->Rotate(yawRot * pitchRot);
        }

        HandleFreeCameraMovement(deltaTime);
    }

    // スクロールズーム（右クリック不要）
    HandleScrollZoom(deltaTime);
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
