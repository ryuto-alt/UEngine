#pragma once
#include "Mymath.h"
#include <vector>
#include <cstdint>

class NavMesh;

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
};

struct StuckDetectionComponent {
    float timer = 0.0f;
    float detectionTime = 2.5f;
    float distanceThreshold = 0.3f;
    bool isRecovering = false;
    int32_t recoveryAttempts = 0;
};

struct EnemyJumpscareComponent {
    bool isJumpscaring = false;
    float timer = 0.0f;
    float duration = 0.0f;
};

struct StealthComponent {
    bool stealthEnabled = false;
    bool stealthActive = false;
    float outOfRangeTimer = 0.0f;
    float stealthAudioRange = 22.0f;
    float stealthActivationTime = 10.0f;
};

} // namespace ECS
