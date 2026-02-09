#pragma once
#include "Mymath.h"

namespace ECS {

struct PlayerTag {};

struct PlayerMovementComponent {
    float moveSpeed = 4.5f;
    float sneakSpeedMultiplier = 0.5f;
    float runSpeedMultiplier = 1.9f;
    bool isMoving = false;
    bool isSneaking = false;
    bool isRunning = false;
    Vector3 moveDirection{0.0f, 0.0f, 0.0f};
    Vector3 smoothedPosition{0.0f, 0.0f, 0.0f};
};

struct PlayerInputComponent {
    bool previousBButtonPressed = false;
};

struct JumpComponent {
    float jumpPower = 10.0f;
};

struct PlayerFootstepComponent {
    float lastFootstepTime = -999.0f;
    Vector3 lastFootstepPosition{0.0f, 0.0f, 0.0f};
    float footstepInterval = 0.5f;
    float footstepTimer = 0.0f;
};

struct JumpscareVictimComponent {
    bool isInJumpscare = false;
};

} // namespace ECS
