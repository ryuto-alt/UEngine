#include "pch.h"
#include "NavAgentComponent.h"
#include "NavMeshManager.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"
#include <cmath>

namespace UnoEngine {

void NavAgentComponent::Awake() {
    state_ = AgentState::Idle;
    currentPath_.clear();
    velocity_ = {0.0f, 0.0f, 0.0f};
}

void NavAgentComponent::Start() {
}

void NavAgentComponent::OnUpdate(float deltaTime) {
    if (!enabled_ || state_ == AgentState::Idle) {
        return;
    }

    if (currentPath_.empty()) {
        state_ = AgentState::Idle;
        return;
    }

    UpdateMovement(deltaTime);
    UpdateRotation(deltaTime);
}

void NavAgentComponent::OnDestroy() {
    ClearPath();
}

bool NavAgentComponent::SetDestination(const DirectX::XMFLOAT3& destination) {
    destination_ = destination;
    
    if (!CalculatePath()) {
        state_ = AgentState::Idle;
        return false;
    }

    state_ = AgentState::Moving;
    currentWaypointIndex_ = 0;
    return true;
}

void NavAgentComponent::Stop() {
    state_ = AgentState::Idle;
    velocity_ = {0.0f, 0.0f, 0.0f};
    currentSpeed_ = 0.0f;
}

void NavAgentComponent::ClearPath() {
    currentPath_.clear();
    currentWaypointIndex_ = 0;
    remainingDistance_ = 0.0f;
    Stop();
}

bool NavAgentComponent::HasReachedDestination() const {
    return state_ == AgentState::Arrived;
}

void NavAgentComponent::UpdateMovement(float deltaTime) {
    if (!gameObject_ || currentPath_.empty()) {
        return;
    }

    auto& transform = gameObject_->GetTransform();
    auto pos = transform.GetPosition();
    DirectX::XMFLOAT3 currentPos = {pos.GetX(), pos.GetY(), pos.GetZ()};

    // 現在のウェイポイントを取得
    if (currentWaypointIndex_ >= currentPath_.size()) {
        state_ = AgentState::Arrived;
        velocity_ = {0.0f, 0.0f, 0.0f};
        currentSpeed_ = 0.0f;
        return;
    }

    const auto& targetWaypoint = currentPath_[currentWaypointIndex_];

    // ウェイポイントへのベクトル
    float dx = targetWaypoint.x - currentPos.x;
    float dy = targetWaypoint.y - currentPos.y;
    float dz = targetWaypoint.z - currentPos.z;
    float distToWaypoint = std::sqrt(dx * dx + dy * dy + dz * dz);

    // 残り距離を計算
    remainingDistance_ = distToWaypoint;
    for (size_t i = currentWaypointIndex_ + 1; i < currentPath_.size(); ++i) {
        const auto& wp1 = currentPath_[i - 1];
        const auto& wp2 = currentPath_[i];
        float segDx = wp2.x - wp1.x;
        float segDy = wp2.y - wp1.y;
        float segDz = wp2.z - wp1.z;
        remainingDistance_ += std::sqrt(segDx * segDx + segDy * segDy + segDz * segDz);
    }

    // ウェイポイントに到達したか
    constexpr float waypointThreshold = 0.2f;
    if (distToWaypoint < waypointThreshold) {
        ++currentWaypointIndex_;
        if (currentWaypointIndex_ >= currentPath_.size()) {
            state_ = AgentState::Arrived;
            velocity_ = {0.0f, 0.0f, 0.0f};
            currentSpeed_ = 0.0f;
            return;
        }
        return;
    }

    // 加速・減速
    float targetSpeed = speed_;
    if (autoBrake_ && remainingDistance_ < stoppingDistance_ * 3.0f) {
        targetSpeed = speed_ * (remainingDistance_ / (stoppingDistance_ * 3.0f));
        targetSpeed = std::max(targetSpeed, 0.5f);
    }

    if (currentSpeed_ < targetSpeed) {
        currentSpeed_ += acceleration_ * deltaTime;
        currentSpeed_ = std::min(currentSpeed_, targetSpeed);
    } else if (currentSpeed_ > targetSpeed) {
        currentSpeed_ -= acceleration_ * deltaTime;
        currentSpeed_ = std::max(currentSpeed_, targetSpeed);
    }

    // 停止距離チェック
    if (remainingDistance_ <= stoppingDistance_) {
        state_ = AgentState::Arrived;
        velocity_ = {0.0f, 0.0f, 0.0f};
        currentSpeed_ = 0.0f;
        return;
    }

    // 移動方向を正規化
    float invDist = 1.0f / distToWaypoint;
    float dirX = dx * invDist;
    float dirY = dy * invDist;
    float dirZ = dz * invDist;

    // 速度を設定
    velocity_.x = dirX * currentSpeed_;
    velocity_.y = dirY * currentSpeed_;
    velocity_.z = dirZ * currentSpeed_;

    // 位置を更新
    float newX = currentPos.x + velocity_.x * deltaTime;
    float newY = currentPos.y + velocity_.y * deltaTime + baseOffset_;
    float newZ = currentPos.z + velocity_.z * deltaTime;

    transform.SetPosition(Vector3(newX, newY, newZ));
}

void NavAgentComponent::UpdateRotation(float deltaTime) {
    if (!gameObject_ || currentPath_.empty() || state_ != AgentState::Moving) {
        return;
    }

    // 速度が十分にあるときのみ回転
    float speedSq = velocity_.x * velocity_.x + velocity_.z * velocity_.z;
    if (speedSq < 0.01f) {
        return;
    }

    auto& transform = gameObject_->GetTransform();

    // 目標方向角度（Y軸回転のみ）
    float targetYaw = std::atan2(velocity_.x, velocity_.z);

    // 現在の回転を取得
    auto currentRot = transform.GetRotation();
    
    // 簡易的なYaw角度取得（クォータニオンからの抽出）
    float sinY = 2.0f * (currentRot.GetW() * currentRot.GetY() - currentRot.GetZ() * currentRot.GetX());
    float cosY = 1.0f - 2.0f * (currentRot.GetX() * currentRot.GetX() + currentRot.GetY() * currentRot.GetY());
    float currentYaw = std::atan2(sinY, cosY);

    // 角度差
    float angleDiff = targetYaw - currentYaw;

    // -PI〜PIに正規化
    constexpr float PI = 3.14159265f;
    while (angleDiff > PI) angleDiff -= 2.0f * PI;
    while (angleDiff < -PI) angleDiff += 2.0f * PI;

    // 回転速度（ラジアン/秒）
    float maxRotation = angularSpeed_ * 0.0174533f * deltaTime;

    // 回転を適用
    if (std::abs(angleDiff) > maxRotation) {
        angleDiff = (angleDiff > 0.0f) ? maxRotation : -maxRotation;
    }

    float newYaw = currentYaw + angleDiff;

    // 新しいクォータニオンを作成（Y軸回転のみ）
    float halfAngle = newYaw * 0.5f;
    Quaternion newRot(0.0f, std::sin(halfAngle), 0.0f, std::cos(halfAngle));
    transform.SetRotation(newRot);
}

bool NavAgentComponent::CalculatePath() {
    if (!gameObject_) {
        return false;
    }

    auto& navMesh = Navigation::NavMeshManager::Get();
    if (!navMesh.IsBuilt()) {
        return false;
    }

    auto pos = gameObject_->GetTransform().GetPosition();
    DirectX::XMFLOAT3 startPos = {pos.GetX(), pos.GetY(), pos.GetZ()};

    currentPath_.clear();
    if (!navMesh.FindPath(startPos, destination_, currentPath_)) {
        return false;
    }

    return !currentPath_.empty();
}

DirectX::XMFLOAT3 NavAgentComponent::GetNextWaypoint() const {
    if (currentWaypointIndex_ < currentPath_.size()) {
        return currentPath_[currentWaypointIndex_];
    }
    return destination_;
}

} // namespace UnoEngine
