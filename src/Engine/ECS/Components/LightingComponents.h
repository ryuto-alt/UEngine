#pragma once
#include "Mymath.h"

namespace ECS {

struct FlashlightComponent {
    float followSmoothness = 0.1f;
    Vector3 smoothedPosition{0.0f, 5.0f, -2.0f};
    Vector3 smoothedDirection{0.0f, -1.0f, 0.3f};
};

struct LightFlickerComponent {
    float flickerTimer = 0.0f;
    float flickerBaseIntensity = 3.5f;
    float flickerAmount = 0.25f;
    float flickerSpeed = 3.0f;

    float blinkTimer = 0.0f;
    float nextBlinkTime = 5.0f;
    bool isBlinking = false;
    float blinkDuration = 0.0f;
    float blinkProgress = 0.0f;

    float fearFlickerIntensity = 0.0f;
};

struct DirectionalLightReceiverTag {};
struct SpotLightReceiverTag {};

} // namespace ECS
