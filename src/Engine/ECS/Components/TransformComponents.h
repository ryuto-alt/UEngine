#pragma once
#include "Mymath.h"

namespace ECS {

struct TransformComponent {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

struct PreviousPositionComponent {
    Vector3 previousPosition{0.0f, 0.0f, 0.0f};
};

struct VelocityComponent {
    Vector3 velocity{0.0f, 0.0f, 0.0f};
};

struct GravityComponent {
    float gravity = -25.0f;
    bool isGrounded = false;
};

struct RotationSmoothingComponent {
    float currentRotationY = 0.0f;
    float targetRotationY = 0.0f;
    float smoothingSpeed = 17.0f;
};

} // namespace ECS
