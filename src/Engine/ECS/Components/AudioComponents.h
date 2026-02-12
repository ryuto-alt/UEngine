#pragma once
#include "Mymath.h"
#include <memory>
#include "SpatialAudioListener.h"
#include "SpatialAudioSource.h"

namespace ECS {

struct AudioListenerComponent {
    std::unique_ptr<SpatialAudioListener> listener;
};

struct EnemyFootstepAudioComponent {
    std::unique_ptr<SpatialAudioSource> footstepSource1;
    std::unique_ptr<SpatialAudioSource> footstepSource2;
    bool useFootstep1 = true;
    float totalTime = 0.0f;

    // Bone tracking
    float previousLeftFootY = 0.0f;
    float previousRightFootY = 0.0f;
    bool leftFootWasAbove = true;
    bool rightFootWasAbove = true;
    float groundThreshold = 0.15f;
    float footLiftThreshold = 0.25f;
    float lastLeftLandTime = -999.0f;
    float lastRightLandTime = -999.0f;
    float cooldown = 0.2f;

    enum class LastFoot { None, Left, Right };
    LastFoot lastFoot = LastFoot::None;
};

struct DetectionSoundComponent {
    std::unique_ptr<SpatialAudioSource> source;
    float lastEndTime = -10.0f;
    bool isPlaying = false;
    float cooldown = 3.0f;
    float range = 20.0f;
};

struct BarkSoundComponent {
    std::unique_ptr<SpatialAudioSource> source;
    float lastBarkTime = -999.0f;
    float nextBarkInterval = 10.0f;
    float minInterval = 8.0f;
    float maxInterval = 12.0f;
    float totalTime = 0.0f;
};

struct ChaseBGMComponent {
    bool loaded = false;
    bool playing = false;
    float volume = 0.0f;
    float targetVolume = 0.0f;
    bool fadingIn = false;
    bool fadingOut = false;
    float maxVolume = 0.075f;
    float fadeInDuration = 2.0f;
    float fadeOutDuration = 3.0f;
};

} // namespace ECS
