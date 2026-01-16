#include "pch.h"
#include "EnemyDetectionComponent.h"
#include "../Core/GameObject.h"
#include "../Core/Scene.h"
#include "../Core/Logger.h"
#include "../Navigation/NavAgentComponent.h"
#include <cmath>

namespace UnoEngine {

void EnemyDetectionComponent::Awake() {
    navAgent_ = GetGameObject()->GetComponent<NavAgentComponent>();
}

void EnemyDetectionComponent::Start() {
    Logger::Info("[EnemyDetection] Start - NavAgent: {}, Scene: {}",
                 navAgent_ ? "OK" : "NULL",
                 scene_ ? "OK" : "NULL");

    if (!scene_) {
        Logger::Warning("[EnemyDetection] Scene is null!");
    }

    if (navAgent_) {
        StartWandering();
    }
}

void EnemyDetectionComponent::OnUpdate(float deltaTime) {
    if (!navAgent_) {
        return;
    }

    if (!scene_) {
        return;
    }

    switch (state_) {
        case State::Idle: {
            // プレイヤーを探す
            auto* target = FindTarget();
            if (target) {
                float dist = GetDistanceToTarget(target);
                bool inFOV = IsTargetInFOV(target);

                if (dist <= detectionRange_ && inFOV) {
                    Logger::Info("[EnemyDetection] Found target! Distance: {:.1f}m, InFOV: true", dist);
                    StartChasing(target);
                }
            }
            break;
        }

        case State::Chasing: {
            if (!currentTarget_) {
                StopChasing();
                break;
            }

            float distance = GetDistanceToTarget(currentTarget_);

            // 見失う距離を超えたら追跡終了
            if (distance > loseRange_) {
                StopChasing();
            }
            break;
        }

        case State::LostTarget: {
            lostTimer_ += deltaTime;
            if (lostTimer_ >= lostWaitTime_) {
                StartWandering();
            }
            break;
        }
    }
}

GameObject* EnemyDetectionComponent::FindTarget() {
    if (!scene_) {
        return nullptr;
    }

    for (auto& obj : scene_->GetGameObjects()) {
        if (obj->GetName() == targetName_) {
            return obj.get();
        }
    }

    return nullptr;
}

bool EnemyDetectionComponent::IsTargetInSight(GameObject* target) {
    if (!target) {
        return false;
    }

    float distance = GetDistanceToTarget(target);
    if (distance > detectionRange_) {
        return false;
    }

    return IsTargetInFOV(target);
}

float EnemyDetectionComponent::GetDistanceToTarget(GameObject* target) {
    if (!target) {
        return std::numeric_limits<float>::max();
    }

    auto myPos = GetGameObject()->GetTransform().GetPosition();
    auto targetPos = target->GetTransform().GetPosition();

    float dx = targetPos.GetX() - myPos.GetX();
    float dy = targetPos.GetY() - myPos.GetY();
    float dz = targetPos.GetZ() - myPos.GetZ();

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool EnemyDetectionComponent::IsTargetInFOV(GameObject* target) {
    if (!target) {
        return false;
    }

    auto& myTransform = GetGameObject()->GetTransform();
    auto myPos = myTransform.GetPosition();
    auto targetPos = target->GetTransform().GetPosition();

    // ターゲットへの方向ベクトル（XZ平面）
    float dx = targetPos.GetX() - myPos.GetX();
    float dz = targetPos.GetZ() - myPos.GetZ();
    float length = std::sqrt(dx * dx + dz * dz);

    if (length < 0.001f) {
        return true; // 同じ位置
    }

    dx /= length;
    dz /= length;

    // 敵の前方ベクトルを取得
    auto forward = myTransform.GetForward();
    float forwardX = forward.GetX();
    float forwardZ = forward.GetZ();

    // XZ平面で正規化
    float forwardLen = std::sqrt(forwardX * forwardX + forwardZ * forwardZ);
    if (forwardLen > 0.001f) {
        forwardX /= forwardLen;
        forwardZ /= forwardLen;
    }

    // 内積で角度を計算
    float dot = dx * forwardX + dz * forwardZ;
    float angleRad = std::acos(std::clamp(dot, -1.0f, 1.0f));
    float angleDeg = angleRad * (180.0f / 3.14159265f);

    // FOVの半分と比較（±45度 = 90度FOV）
    return angleDeg <= (fieldOfView_ * 0.5f);
}

void EnemyDetectionComponent::StartChasing(GameObject* target) {
    currentTarget_ = target;
    state_ = State::Chasing;
    // StartChaseが内部で状態を切り替えるのでStopWanderは不要
    navAgent_->StartChase(target, 0.2f);
    Logger::Info("[EnemyDetection] Start chasing: {}", target->GetName());
}

void EnemyDetectionComponent::StopChasing() {
    currentTarget_ = nullptr;
    state_ = State::LostTarget;
    lostTimer_ = 0.0f;
    navAgent_->StopChase(); // 内部でStop()も呼ばれる
    Logger::Info("[EnemyDetection] Lost target, waiting...");
}

void EnemyDetectionComponent::StartWandering() {
    state_ = State::Idle;
    lostTimer_ = 0.0f;
    navAgent_->StartWander(NavAgentComponent::WanderMode::AroundSpawn, wanderRadius_);
    Logger::Info("[EnemyDetection] Start wandering");
}

} // namespace UnoEngine
