#include "EnemyFootstepAudioSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/AudioComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include <cmath>
#include <algorithm>

namespace ECS {

namespace {

// Horror-style non-linear volume falloff
float CalculateHorrorVolume(float distance) {
    constexpr float kClose = 5.0f, kMid = 15.0f, kFar = 30.0f;
    if (distance < kClose) return 1.2f;
    if (distance < kMid) {
        float t = (distance - kClose) / (kMid - kClose);
        return 1.2f * std::pow(0.3f, t);
    }
    if (distance < kFar) {
        float t = (distance - kMid) / (kFar - kMid);
        return 0.36f * (1.0f - t);
    }
    return 0.0f;
}

} // anonymous namespace

void EnemyFootstepAudioSystem::Update(World& world, float deltaTime) {
    // Get listener from player entity
    Entity playerEntity = world.FindEntityWith<PlayerTag>();
    if (!playerEntity.IsValid()) return;
    if (!world.HasComponent<AudioListenerComponent>(playerEntity)) return;
    auto& listenerComp = world.GetComponent<AudioListenerComponent>(playerEntity);
    if (!listenerComp.listener) return;
    Vector3 listenerPos = listenerComp.listener->GetPosition();
    Vector3 listenerFwd = listenerComp.listener->GetForward();

    world.ForEach<EnemyTag, TransformComponent, EnemyAIComponent, StealthComponent,
                  AnimatedModelComponent, EnemyFootstepAudioComponent>(
        [deltaTime, &listenerPos, &listenerFwd](Entity entity, EnemyTag&, TransformComponent& transform,
                   EnemyAIComponent& ai, StealthComponent& stealth,
                   AnimatedModelComponent& anim, EnemyFootstepAudioComponent& audio) {

            if (!ai.isActive) return;
            if (!anim.animatedModel || anim.animationPaused) return;

            // Only during Walk/Run animations
            std::string currentAnimName = anim.animatedModel->GetCurrentAnimationName();
            if (currentAnimName != "Walk" && currentAnimName != "Run") return;

            // Stealth: if active, skip all footstep sounds
            if (stealth.stealthEnabled && stealth.stealthActive) return;

            // Get foot bone positions from skeleton
            const Skeleton& skeleton = anim.animatedModel->GetSkeleton();
            auto leftFootIt = skeleton.jointMap.find("mixamorig:LeftToeBase");
            auto rightFootIt = skeleton.jointMap.find("mixamorig:RightToeBase");
            if (leftFootIt == skeleton.jointMap.end())
                leftFootIt = skeleton.jointMap.find("mixamorig:LeftFoot");
            if (rightFootIt == skeleton.jointMap.end())
                rightFootIt = skeleton.jointMap.find("mixamorig:RightFoot");
            if (leftFootIt == skeleton.jointMap.end() || rightFootIt == skeleton.jointMap.end()) return;

            const Joint& leftJoint = skeleton.joints[leftFootIt->second];
            const Joint& rightJoint = skeleton.joints[rightFootIt->second];

            constexpr float modelScale = 0.05f;
            float worldLeftY = transform.position.y + leftJoint.skeletonSpaceMatrix.m[3][1] * modelScale;
            float worldRightY = transform.position.y + rightJoint.skeletonSpaceMatrix.m[3][1] * modelScale;

            constexpr float GROUND_HEIGHT = 0.0f;

            audio.totalTime += deltaTime;

            // Landing detection
            bool leftAbove = worldLeftY > GROUND_HEIGHT + audio.groundThreshold;
            bool rightAbove = worldRightY > GROUND_HEIGHT + audio.groundThreshold;
            bool leftLanded = audio.leftFootWasAbove && !leftAbove;
            bool rightLanded = audio.rightFootWasAbove && !rightAbove;

            // Left foot landing
            if (leftLanded) {
                float timeSinceAny = (std::min)(audio.totalTime - audio.lastLeftLandTime,
                                              audio.totalTime - audio.lastRightLandTime);
                bool canPlay = (audio.lastFoot != EnemyFootstepAudioComponent::LastFoot::Left)
                               && (timeSinceAny > audio.cooldown);
                if (canPlay && audio.footstepSource1) {
                    audio.footstepSource1->SetPosition(transform.position);
                    audio.footstepSource1->Update(listenerPos, listenerFwd);

                    Vector3 diff = {listenerPos.x - transform.position.x,
                                    listenerPos.y - transform.position.y,
                                    listenerPos.z - transform.position.z};
                    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                    audio.footstepSource1->SetVolume(CalculateHorrorVolume(dist));
                    audio.footstepSource1->Play(false);

                    audio.lastLeftLandTime = audio.totalTime;
                    audio.lastFoot = EnemyFootstepAudioComponent::LastFoot::Left;
                }
            }
            audio.leftFootWasAbove = leftAbove;

            // Right foot landing
            if (rightLanded) {
                float timeSinceAny = (std::min)(audio.totalTime - audio.lastLeftLandTime,
                                              audio.totalTime - audio.lastRightLandTime);
                bool canPlay = (audio.lastFoot != EnemyFootstepAudioComponent::LastFoot::Right)
                               && (timeSinceAny > audio.cooldown);
                if (canPlay && audio.footstepSource2) {
                    audio.footstepSource2->SetPosition(transform.position);
                    audio.footstepSource2->Update(listenerPos, listenerFwd);

                    Vector3 diff = {listenerPos.x - transform.position.x,
                                    listenerPos.y - transform.position.y,
                                    listenerPos.z - transform.position.z};
                    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                    audio.footstepSource2->SetVolume(CalculateHorrorVolume(dist));
                    audio.footstepSource2->Play(false);

                    audio.lastRightLandTime = audio.totalTime;
                    audio.lastFoot = EnemyFootstepAudioComponent::LastFoot::Right;
                }
            }
            audio.rightFootWasAbove = rightAbove;
        }
    );
}

} // namespace ECS
