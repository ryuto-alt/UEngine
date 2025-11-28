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

    // 右クリック中はホバー状態に関係なく操作を継続
    if (!viewportHovered_ && !rightMousePressed_) {
        isControlling_ = false;
        return;
    }

    if (!viewportHovered_ && rightMousePressed_ && !rightMouseDown) {
        // ビューポート外で右クリックを離した
        rightMousePressed_ = false;
        isControlling_ = false;
        while (ShowCursor(TRUE) < 0);
        return;
    }

    if (rightMouseDown && !rightMousePressed_) {
        // 右クリック開始
        rightMousePressed_ = true;
        isControlling_ = true;
        while (ShowCursor(FALSE) >= 0);
        GetCursorPos(&lockMousePos_);

        // 現在のカメラの向きからyaw/pitchを計算
        Vector3 forward = camera_->GetForward();
        yaw_ = std::atan2(forward.GetX(), forward.GetZ());
        pitch_ = std::asin(-forward.GetY());

        // オービットターゲットがある場合、オービット用の角度も計算
        if (hasOrbitTarget_) {
            Vector3 toCamera = camera_->GetPosition() - orbitTarget_;
            orbitDistance_ = toCamera.Length();
            if (orbitDistance_ < 0.1f) orbitDistance_ = 5.0f;

            // 球面座標からyaw/pitchを計算
            Vector3 dir = toCamera.Normalize();
            orbitYaw_ = std::atan2(dir.GetX(), dir.GetZ());
            orbitPitch_ = std::asin(dir.GetY());
        }
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

        if (hasOrbitTarget_) {
            // SHIFT+右クリック: オービットターゲットの高さを調整
            if (io.KeyShift) {
                // マウスの上下移動でターゲットのY座標を調整
                float heightAdjust = deltaY * moveSpeed_ * deltaTime * 0.5f;
                orbitTarget_.SetY(orbitTarget_.GetY() + heightAdjust);
                
                // カメラ位置もY方向に同じだけ移動（オービット角度は保持）
                Vector3 currentPos = camera_->GetPosition();
                currentPos.SetY(currentPos.GetY() + heightAdjust);
                camera_->SetPosition(currentPos);
                
                // カメラをターゲットに向ける
                Matrix4x4 viewMat = Matrix4x4::LookAtLH(currentPos, orbitTarget_, Vector3::UnitY());
                Quaternion rot = Quaternion::FromRotationMatrix(viewMat.Inverse());
                camera_->SetRotation(rot);
            } else {
                // 通常のオービット回転
                orbitYaw_ += deltaX * rotateSpeed_ * deltaTime;
                orbitPitch_ += deltaY * rotateSpeed_ * deltaTime;

                // ピッチ制限
                const float maxPitch = 1.5f;
                if (orbitPitch_ > maxPitch) orbitPitch_ = maxPitch;
                if (orbitPitch_ < -maxPitch) orbitPitch_ = -maxPitch;

                // 球面座標からカメラ位置を計算
                float x = orbitDistance_ * std::cos(orbitPitch_) * std::sin(orbitYaw_);
                float y = orbitDistance_ * std::sin(orbitPitch_);
                float z = orbitDistance_ * std::cos(orbitPitch_) * std::cos(orbitYaw_);

                Vector3 newPos = orbitTarget_ + Vector3(x, y, z);
                camera_->SetPosition(newPos);

                // カメラをターゲットに向ける
                Vector3 lookDir = (orbitTarget_ - newPos).Normalize();
                Vector3 right = Vector3::UnitY().Cross(lookDir).Normalize();
                Vector3 up = lookDir.Cross(right).Normalize();

                // View行列から回転を設定
                Matrix4x4 viewMat = Matrix4x4::LookAtLH(newPos, orbitTarget_, Vector3::UnitY());
                Quaternion rot = Quaternion::FromRotationMatrix(viewMat.Inverse());
                camera_->SetRotation(rot);
            }
        } else {
            // フリーカメラ回転
            yaw_ += deltaX * rotateSpeed_ * deltaTime;
            pitch_ += deltaY * rotateSpeed_ * deltaTime;

            // ピッチ制限
            const float maxPitch = 1.5f;
            if (pitch_ > maxPitch) pitch_ = maxPitch;
            if (pitch_ < -maxPitch) pitch_ = -maxPitch;

            // Quaternionで回転を設定
            Quaternion rot = Quaternion::RotationRollPitchYaw(pitch_, yaw_, 0.0f);
            camera_->SetRotation(rot);
        }
    }

    // WASD移動は常に有効（右クリック不要）
    HandleFreeCameraMovement(deltaTime);
    HandleScrollZoom(deltaTime);
}

void EditorCamera::HandleFreeCameraMovement(float deltaTime) {
    if (!camera_) return;

    ImGuiIO& io = ImGui::GetIO();
    
    // Scene Viewがホバーされていない場合はWASD移動を無効化
    if (!viewportHovered_) return;

    // カメラの実際の向きから移動方向を計算（FPSスタイル）
    Vector3 camForward = camera_->GetForward();

    // 水平面に投影して正規化
    Vector3 forward(camForward.GetX(), 0.0f, camForward.GetZ());
    if (forward.Length() > 0.001f) {
        forward = forward.Normalize();
    } else {
        forward = Vector3(0.0f, 0.0f, 1.0f);
    }

    // 右方向はY軸との外積で求める
    Vector3 right = Vector3::UnitY().Cross(forward).Normalize();

    Vector3 movement(0.0f, 0.0f, 0.0f);

    if (ImGui::IsKeyDown(ImGuiKey_W)) movement = movement + forward;
    if (ImGui::IsKeyDown(ImGuiKey_S)) movement = movement - forward;
    if (ImGui::IsKeyDown(ImGuiKey_A)) movement = movement - right;
    if (ImGui::IsKeyDown(ImGuiKey_D)) movement = movement + right;
    if (ImGui::IsKeyDown(ImGuiKey_Space)) movement = movement + Vector3::UnitY();
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) movement = movement - Vector3::UnitY();

    float speed = moveSpeed_;

    if (movement.Length() > 0.001f) {
        // WASD移動時はオービットモードを解除してフリーカメラに戻る
        if (hasOrbitTarget_) {
            hasOrbitTarget_ = false;
        }
        movement = movement.Normalize() * speed * deltaTime;
        camera_->Translate(movement);
    }
}

void EditorCamera::HandleMouseRotation(float deltaTime) {
    // 未使用
}

void EditorCamera::HandleScrollZoom(float deltaTime) {
    if (!camera_) return;

    ImGuiIO& io = ImGui::GetIO();
    float scroll = io.MouseWheel;

    if (std::abs(scroll) > 0.001f) {
        if (hasOrbitTarget_) {
            // オービット距離を変更
            orbitDistance_ -= scroll * scrollSpeed_;
            if (orbitDistance_ < 1.0f) orbitDistance_ = 1.0f;
            if (orbitDistance_ > 100.0f) orbitDistance_ = 100.0f;

            // 位置を更新
            float x = orbitDistance_ * std::cos(orbitPitch_) * std::sin(orbitYaw_);
            float y = orbitDistance_ * std::sin(orbitPitch_);
            float z = orbitDistance_ * std::cos(orbitPitch_) * std::cos(orbitYaw_);
            camera_->SetPosition(orbitTarget_ + Vector3(x, y, z));
        } else {
            Vector3 forward = camera_->GetForward();
            camera_->Translate(forward * scroll * scrollSpeed_);
        }
    }
}

void EditorCamera::FocusOn(const Vector3& targetPosition, float distance) {
    if (!camera_) return;

    orbitTarget_ = targetPosition;
    orbitDistance_ = distance;
    hasOrbitTarget_ = true;

    // 現在のカメラからターゲットへの角度を計算
    Vector3 toCamera = camera_->GetPosition() - targetPosition;
    if (toCamera.Length() < 0.1f) {
        toCamera = Vector3(0.0f, 1.0f, 3.0f);
    }

    Vector3 dir = toCamera.Normalize();
    orbitYaw_ = std::atan2(dir.GetX(), dir.GetZ());
    orbitPitch_ = std::asin(dir.GetY());

    // カメラ位置を設定
    float x = orbitDistance_ * std::cos(orbitPitch_) * std::sin(orbitYaw_);
    float y = orbitDistance_ * std::sin(orbitPitch_);
    float z = orbitDistance_ * std::cos(orbitPitch_) * std::cos(orbitYaw_);

    Vector3 newPos = orbitTarget_ + Vector3(x, y, z);
    camera_->SetPosition(newPos);

    // カメラをターゲットに向ける
    Matrix4x4 viewMat = Matrix4x4::LookAtLH(newPos, orbitTarget_, Vector3::UnitY());
    Quaternion rot = Quaternion::FromRotationMatrix(viewMat.Inverse());
    camera_->SetRotation(rot);
}

} // namespace UnoEngine
