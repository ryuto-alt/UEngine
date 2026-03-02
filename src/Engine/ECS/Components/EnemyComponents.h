#pragma once
#include "Mymath.h"
#include <vector>
#include <cstdint>
#include <memory>

class NavMesh;
class LineRenderer;

// Forward declare EnemyAIConfig if header not included
struct EnemyAIConfig;

namespace ECS {

struct EnemyTag {};

struct EnemyAIComponent {
    float intelligence = 8.0f;
    float aggressiveness = 20.0f;
    float mobility = 6.5f;
    float patrolMobility = 4.5f;
    float searchMobility = 5.5f;

    float moveSpeed = 8.0f;
    float patrolMoveSpeed = 4.5f;
    float searchMoveSpeed = 5.5f;
    bool isChasing = false;
    bool wasChasing = false;
    bool isSearching = false;
    bool isActive = false;
    float searchTimer = 0.0f;
    float maxSearchTime = 15.0f;
};

struct VisionComponent {
    float visionRange = 27.0f;
    float visionAngle = 90.0f;
    float detectionDistance = 18.0f;
    float stealthDetectionDistance = 10.0f;
    float proximityDetectionDistance = 5.0f;
    float chaseReleaseDistance = 25.0f;
    float lostSightGracePeriod = 7.0f;
    Vector3 lastSeenPlayerPosition{0.0f, 0.0f, 0.0f};
    float lostSightTimer = 0.0f;
};

struct SoundDetectionComponent {
    float soundDetectionRange = 30.0f;
    Vector3 lastHeardPosition{0.0f, 0.0f, 0.0f};
    float lastSoundTime = -999.0f;
    float soundReactionTime = 0.5f;
};

struct PathfindingComponent {
    NavMesh* navMesh = nullptr;
    std::vector<Vector3> currentPath;
    int32_t waypointIndex = 0;
    float updateTimer = 0.0f;
    float updateInterval = 0.5f;
    bool isAtCorner = false;
    float cornerSlowdown = 1.0f;
    float currentSpeed = 0.0f;
};

struct StuckDetectionComponent {
    float timer = 0.0f;
    float detectionTime = 2.5f;
    float distanceThreshold = 1.0f; // Total distance over detectionTime period
    bool isRecovering = false;
    int32_t recoveryAttempts = 0;
    Vector3 checkOrigin{0.0f, 0.0f, 0.0f}; // Position when tracking started
    bool isTracking = false;
};

struct EnemyJumpscareComponent {
    bool isJumpscaring = false;
    float timer = 0.0f;
    float duration = 0.0f;
    Vector3 lockedEnemyPos{0.0f, 0.0f, 0.0f};
    Vector3 lockedPlayerPos{0.0f, 0.0f, 0.0f};
    Vector3 lockedCamPos{0.0f, 0.0f, 0.0f};
    Vector3 lockedCamRot{0.0f, 0.0f, 0.0f};
};

struct StealthComponent {
    bool stealthEnabled = false;
    bool stealthActive = false;
    float outOfRangeTimer = 0.0f;
    float stealthAudioRange = 22.0f;
    float stealthActivationTime = 5.0f;

    // 復帰猶予（Continue後の安全時間）
    bool isRevivalGrace = false;
    float revivalGraceTimer = 0.0f;
    static constexpr float kRevivalGraceDuration = 10.0f;
};

struct AmbushWarpComponent {
    float safetyTimer = 0.0f;
    float warpCooldownTimer = 0.0f;

    // Post-warp slowdown
    float slowdownTimer = 0.0f;
    static constexpr float kSlowdownDuration = 3.0f;
    static constexpr float kSlowdownMultiplier = 0.5f;

    static constexpr float kSafetyThreshold = 20.0f;
    static constexpr float kWarpCooldown = 45.0f;
    static constexpr float kMinEnemyDistance = 22.0f;
    static constexpr float kWarpDistance = 12.0f;
    static constexpr float kMinWarpDistance = 8.0f;
    static constexpr float kRandomChancePerSec = 0.25f;
};

struct EnemyDebugComponent {
    std::unique_ptr<LineRenderer> lineRenderer;
    bool drawFootBones = true;
};

} // namespace ECS
