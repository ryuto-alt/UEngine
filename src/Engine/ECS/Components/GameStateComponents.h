#pragma once
#include "Mymath.h"
#include <cstdint>

namespace ECS {

struct GameStateComponent {
    bool isGameOver = false;
    int32_t captureCount = 0;
    int32_t maxCaptures = 3;
    bool jumpscareStarted = false;
    bool allOrbsCollected = false;
    float endingFadeTimer = 0.0f;
};

struct RespawnStateComponent {
    enum class State { None, FadeOut, Respawning, FadeIn };
    State state = State::None;
    float timer = 0.0f;
    float fadeDuration = 1.0f;
    float fadeAlpha = 0.0f;
    float respawnWaitTime = 0.5f;
    Vector3 playerInitialPosition{0.0f, 0.0f, 0.0f};
    Vector3 enemyInitialPosition{50.0f, 0.0f, 0.0f};
};

struct FearEffectComponent {
    float vignetteIntensity = 0.0f;
    float shakeIntensity = 0.0f;
    float flickerIntensity = 0.0f;
};

struct TutorialComponent {
    bool isActive = true;
    bool isFinished = false;
};

struct StealthTutorialComponent {
    bool triggered = false;   // Already shown once
    bool active = false;      // Currently showing subtitle
};

} // namespace ECS
