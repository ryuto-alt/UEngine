#pragma once

#include "Component.h"
#include "Camera.h"
#include "../Math/Math.h"

namespace UnoEngine {

class InputManager;

class OrbitController : public Component {
public:
    OrbitController() = default;

    void OnUpdate(float deltaTime) override;

    void SetTarget(const Vector3& target) { target_ = target; }
    void SetDistance(float distance) { distance_ = distance; }
    void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }
    void SetZoomSpeed(float speed) { zoomSpeed_ = speed; }

    Vector3 GetTarget() const { return target_; }
    float GetDistance() const { return distance_; }

private:

    Vector3 target_ = Vector3::Zero();
    float distance_ = 5.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    float rotationSpeed_ = 0.005f;
    float zoomSpeed_ = 1.0f;
    float minDistance_ = 1.0f;
    float maxDistance_ = 20.0f;
};

} // namespace UnoEngine
