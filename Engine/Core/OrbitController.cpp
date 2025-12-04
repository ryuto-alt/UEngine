#include "pch.h"
#include "OrbitController.h"
#include "SceneManager.h"
#include "../Input/InputManager.h"
#include <algorithm>

namespace UnoEngine {

void OrbitController::OnUpdate(float deltaTime) {
    if (!input_) return;

    const auto& mouse = input_->GetMouse();

    // Rotation with right mouse button
    if (mouse.IsDown(MouseButton::Right)) {
        float deltaX = static_cast<float>(mouse.GetDeltaX());
        float deltaY = static_cast<float>(mouse.GetDeltaY());

        yaw_ -= deltaX * rotationSpeed_;
        pitch_ -= deltaY * rotationSpeed_;

        pitch_ = std::clamp(pitch_, -Math::ToRadians(89.0f), Math::ToRadians(89.0f));
    }

    // Zoom with mouse wheel
    float wheel = static_cast<float>(mouse.GetWheelDelta());
    if (wheel != 0.0f) {
        distance_ -= wheel * zoomSpeed_;
        distance_ = std::clamp(distance_, minDistance_, maxDistance_);
    }

    // Calculate camera position
    float cosYaw = std::cos(yaw_);
    float sinYaw = std::sin(yaw_);
    float cosPitch = std::cos(pitch_);
    float sinPitch = std::sin(pitch_);

    Vector3 offset(
        cosPitch * sinYaw * distance_,
        sinPitch * distance_,
        cosPitch * cosYaw * distance_
    );

    Vector3 cameraPos = target_ + offset;
    camera_.SetPosition(cameraPos);

    // Look at target
    Vector3 forward = (target_ - cameraPos).Normalize();
    Vector3 right = Vector3::UnitY().Cross(forward).Normalize();
    Vector3 up = forward.Cross(right);

    Quaternion rot = Quaternion::LookRotation(forward, up);
    camera_.SetRotation(rot);
}

} // namespace UnoEngine
