#include "pch.h"
#include "EnemyDetectionComponent.h"
#include "EnemyState.h"
#include "../Core/GameObject.h"
#include "../Core/Scene.h"
#include "../Core/Logger.h"
#include "../Navigation/NavAgentComponent.h"
#include <cmath>
#include <algorithm>

namespace UnoEngine {

// ─────────────────────────────────────────────
// EnemyDetectionComponent
// ─────────────────────────────────────────────

EnemyDetectionComponent::EnemyDetectionComponent()
    : currentState_(std::make_unique<IdleState>()) {}

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
    if (!navAgent_ || !scene_) {
        return;
    }

    if (auto nextState = currentState_->Update(*this, deltaTime)) {
        currentState_->Exit(*this);
        nextState->Enter(*this);
        currentState_ = std::move(nextState);
    }
}

EnemyStateType EnemyDetectionComponent::GetStateType() const {
    if (currentState_) {
        return currentState_->GetType();
    }
    return EnemyStateType::Idle;
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

    float dx = targetPos.GetX() - myPos.GetX();
    float dz = targetPos.GetZ() - myPos.GetZ();
    float length = std::sqrt(dx * dx + dz * dz);

    if (length < kNormalizationEpsilon) {
        return true;
    }

    dx /= length;
    dz /= length;

    auto forward = myTransform.GetForward();
    float forwardX = forward.GetX();
    float forwardZ = forward.GetZ();

    float forwardLen = std::sqrt(forwardX * forwardX + forwardZ * forwardZ);
    if (forwardLen > kNormalizationEpsilon) {
        forwardX /= forwardLen;
        forwardZ /= forwardLen;
    }

    float dot = dx * forwardX + dz * forwardZ;
    float angleDeg = std::acos(std::clamp(dot, -1.0f, 1.0f)) * kRadToDeg;

    return angleDeg <= (fieldOfView_ * 0.5f);
}

void EnemyDetectionComponent::StartChasing(GameObject* target) {
    currentTarget_ = target;
    navAgent_->StartChase(target, chaseStoppingDistance_);
    Logger::Info("[EnemyDetection] Start chasing: {}", target->GetName());
}

void EnemyDetectionComponent::StopChasing() {
    currentTarget_ = nullptr;
    navAgent_->StopChase();
    Logger::Info("[EnemyDetection] Lost target, waiting...");
}

void EnemyDetectionComponent::StartWandering() {
    navAgent_->StartWander(NavAgentComponent::WanderMode::AroundSpawn, wanderRadius_);
    Logger::Info("[EnemyDetection] Start wandering");
}

// ─────────────────────────────────────────────
// State implementations
// ─────────────────────────────────────────────

// IdleState

void IdleState::Enter(EnemyDetectionComponent& owner) {
    owner.SetCurrentTarget(nullptr);
    owner.StartWandering();
}

auto IdleState::Update(EnemyDetectionComponent& owner, float /*deltaTime*/)
    -> std::unique_ptr<EnemyState> {
    auto* target = owner.FindTarget();
    if (!target) {
        return nullptr;
    }

    float dist = owner.GetDistanceToTarget(target);
    bool inFOV = owner.IsTargetInFOV(target);

    if (dist <= owner.GetDetectionRange() && inFOV) {
        Logger::Info("[EnemyDetection] Found target! Distance: {:.1f}m, InFOV: true", dist);
        auto nextState = std::make_unique<ChasingState>();
        owner.StartChasing(target);
        return nextState;
    }

    return nullptr;
}

// ChasingState

void ChasingState::Enter(EnemyDetectionComponent& /*owner*/) {}

auto ChasingState::Update(EnemyDetectionComponent& owner, float /*deltaTime*/)
    -> std::unique_ptr<EnemyState> {
    if (!owner.GetCurrentTarget()) {
        return std::make_unique<LostTargetState>();
    }

    float distance = owner.GetDistanceToTarget(owner.GetCurrentTarget());

    if (distance > owner.GetLoseRange()) {
        owner.StopChasing();
        return std::make_unique<LostTargetState>();
    }

    return nullptr;
}

// LostTargetState

void LostTargetState::Enter(EnemyDetectionComponent& /*owner*/) {
    timer_ = 0.0f;
}

auto LostTargetState::Update(EnemyDetectionComponent& owner, float deltaTime)
    -> std::unique_ptr<EnemyState> {
    timer_ += deltaTime;

    if (timer_ >= owner.GetLostWaitTime()) {
        return std::make_unique<IdleState>();
    }

    return nullptr;
}

} // namespace UnoEngine
