#pragma once
#include "Mymath.h"

struct SpotLight;

namespace ECS {

struct CollectibleTag {};

struct OrbComponent {
    bool isCollected = false;
    float collisionRadius = 0.5f;
};

struct FloatingAnimationComponent {
    float floatTimer = 0.0f;
    float floatSpeed = 2.0f;
    float floatAmplitude = 0.3f;
    Vector3 basePosition{0.0f, 0.0f, 0.0f};
};

struct SpotLightGlowComponent {
    bool isIlluminated = false;
    float glowIntensity = 0.0f;
    const SpotLight* currentSpotLight = nullptr;
};

struct RotatingComponent {
    float rotationSpeed = 2.0f;
};

} // namespace ECS
