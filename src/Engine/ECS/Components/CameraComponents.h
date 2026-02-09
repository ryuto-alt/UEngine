#pragma once
#include "Mymath.h"
#include <memory>
#include "GameObject/FPSCamera.h"

namespace ECS {

struct FPSCameraComponent {
    std::unique_ptr<FPSCamera> fpsCamera;
    bool isFPSMode = true;
};

struct CameraFollowComponent {
    float orbitDistance = 3.0f;
    float orbitHeight = 2.5f;
    float smoothingFactor = 0.15f;
};

struct CameraShakeComponent {
    Vector3 shakeOffset{0.0f, 0.0f, 0.0f};
    float fearShakeIntensity = 0.0f;
};

} // namespace ECS
