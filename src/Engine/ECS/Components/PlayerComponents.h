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
    bool isSprinting = false;
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

struct SprintComponent {
    bool  isSprinting      = false;
    float remainingDuration = 0.0f;   // 7秒カウントダウン
    float effectIntensity  = 0.0f;   // 0~1フェード済みの強度
    float sprintTime       = 0.0f;   // 経過時間（シェーダー用）
    float baseFov          = 0.0f;   // スプリント開始時のFOV

    static constexpr float kDuration      = 10.0f;
    static constexpr float kFadeDuration  = 0.4f;
    static constexpr float kFovBoost      = 0.10f;  // radians
    static constexpr float kSpeedMultiplier = 2.28f; // runSpeed * 1.2
};

struct EnemySenseComponent {
    bool  isActive          = false;
    float remainingActive   = 0.0f;  // 3秒カウントダウン
    float remainingCooldown = 0.0f;  // 20秒クールダウン
    float fadeAlpha         = 0.0f;  // 現在の描画アルファ(0~1)

    static constexpr float kActiveDuration   = 3.0f;
    static constexpr float kCooldownDuration = 20.0f;
    static constexpr float kFadeDuration     = 0.5f;  // フェードイン/アウト時間
};

} // namespace ECS
