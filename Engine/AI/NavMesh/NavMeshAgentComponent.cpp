#include "pch.h"
#include "NavMeshAgentComponent.h"
#include "NavMeshSystem.h"
#include "../../Core/GameObject.h"
#include "../../Core/Transform.h"
#include "../../Core/Logger.h"
#include "../../Rendering/DebugRenderer.h"
#include "../../Math/MathCommon.h"
#include <cmath>

namespace UnoEngine {

using namespace DirectX;

namespace {
    float Length(const XMFLOAT3& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    XMFLOAT3 Normalize(const XMFLOAT3& v) {
        float len = Length(v);
        if (len < 0.0001f) return {0, 0, 0};
        return {v.x / len, v.y / len, v.z / len};
    }

    float Distance(const XMFLOAT3& a, const XMFLOAT3& b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float dz = b.z - a.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    float DistanceXZ(const XMFLOAT3& a, const XMFLOAT3& b) {
        float dx = b.x - a.x;
        float dz = b.z - a.z;
        return std::sqrt(dx * dx + dz * dz);
    }
}

void NavMeshAgentComponent::Awake() {
    // 初期位置を同期
    if (auto* go = GetGameObject()) {
        auto& transform = go->GetTransform();
        auto pos = transform.GetPosition();
        m_currentPosition = {pos.GetX(), pos.GetY(), pos.GetZ()};
        m_positionSynced = true;
    }
}

void NavMeshAgentComponent::OnUpdate(float deltaTime) {
    if (!m_positionSynced) {
        // 位置が同期されていない場合、Transformから取得
        if (auto* go = GetGameObject()) {
            auto& transform = go->GetTransform();
            auto pos = transform.GetPosition();
            m_currentPosition = {pos.GetX(), pos.GetY(), pos.GetZ()};
            
            // 現在の回転を同期（QuaternionからY軸回転角を抽出）
            auto rot = transform.GetLocalRotation();
            // Quaternionからオイラー角のYaw成分を抽出
            float x = rot.GetX(), y = rot.GetY(), z = rot.GetZ(), w = rot.GetW();
            m_currentRotationY = std::atan2(2.0f * (w * y + x * z), 1.0f - 2.0f * (y * y + z * z));
        }
    }

    UpdatePathFollowing(deltaTime);
    
    // Transform自動更新モード
    if (m_updateTransform && m_isMoving) {
        if (auto* go = GetGameObject()) {
            auto& transform = go->GetTransform();
            
            // 位置更新
            auto pos = transform.GetPosition();
            pos.SetX(pos.GetX() + m_desiredVelocity.x * deltaTime);
            pos.SetZ(pos.GetZ() + m_desiredVelocity.z * deltaTime);
            transform.SetPosition(pos);
            
            // 回転更新（Y軸回転のQuaternionを作成）
            auto yawQuat = Quaternion::RotationAxis(Vector3::UnitY(), m_currentRotationY);
            transform.SetLocalRotation(yawQuat);
            
            // 内部状態も更新
            m_currentPosition = {pos.GetX(), pos.GetY(), pos.GetZ()};
        }
    }
    
    m_positionSynced = false;  // 次フレームで再同期が必要
}

bool NavMeshAgentComponent::SetDestination(const XMFLOAT3& target) {
    auto& navSystem = NavMeshSystem::GetInstance();
    
    if (!navSystem.HasNavMesh()) {
        Logger::Warning("[NavMeshAgent] SetDestination failed: No NavMesh");
        return false;
    }

    // パス検索
    m_currentPath = navSystem.FindPath(m_currentPosition, target);
    
    if (!m_currentPath.IsValid()) {
        Logger::Warning("[NavMeshAgent] SetDestination failed: Path not found");
        return false;
    }

    m_destination = target;
    m_currentWaypointIndex = 0;
    m_hasPath = true;
    m_isMoving = true;
    m_currentSpeed = 0.0f;

    return true;
}

void NavMeshAgentComponent::Stop() {
    m_isMoving = false;
    m_currentSpeed = 0.0f;
    m_desiredVelocity = {0, 0, 0};
}

void NavMeshAgentComponent::ResetPath() {
    m_currentPath.Clear();
    m_currentWaypointIndex = 0;
    m_hasPath = false;
    m_isMoving = false;
    m_currentSpeed = 0.0f;
    m_desiredVelocity = {0, 0, 0};
}

bool NavMeshAgentComponent::HasReachedDestination() const {
    if (!m_hasPath) return false;
    return DistanceXZ(m_currentPosition, m_destination) <= m_stoppingDistance;
}

float NavMeshAgentComponent::GetRemainingDistance() const {
    if (!m_hasPath || m_currentPath.waypoints.empty())
        return 0.0f;

    float totalDist = 0.0f;
    
    // 現在位置から現在のウェイポイントまで
    if (m_currentWaypointIndex < static_cast<int>(m_currentPath.waypoints.size())) {
        totalDist += Distance(m_currentPosition, m_currentPath.waypoints[m_currentWaypointIndex]);
    }

    // 残りのウェイポイント間の距離
    for (size_t i = m_currentWaypointIndex; i + 1 < m_currentPath.waypoints.size(); ++i) {
        totalDist += Distance(m_currentPath.waypoints[i], m_currentPath.waypoints[i + 1]);
    }

    return totalDist;
}

void NavMeshAgentComponent::SyncPosition(const XMFLOAT3& worldPosition) {
    m_currentPosition = worldPosition;
    m_positionSynced = true;
}

bool NavMeshAgentComponent::SnapToNavMesh() {
    auto& navSystem = NavMeshSystem::GetInstance();
    
    auto snapped = navSystem.SnapToNavMesh(m_currentPosition, m_radius * 2.0f);
    if (snapped) {
        m_currentPosition = *snapped;
        return true;
    }
    return false;
}

void NavMeshAgentComponent::UpdatePathFollowing(float deltaTime) {
    if (!m_hasPath || !m_isMoving || m_currentPath.waypoints.empty()) {
        m_desiredVelocity = {0, 0, 0};
        m_currentSpeed = 0.0f;
        return;
    }
    
    auto& navSystem = NavMeshSystem::GetInstance();
    if (m_autoRepath && navSystem.HasNavMesh() && !navSystem.IsPointOnNavMesh(m_currentPosition)) {
        if (SnapToNavMesh()) {
            SetDestination(m_destination);
            return;
        }
    }

    if (HasReachedDestination()) {
        Stop();
        return;
    }

    XMFLOAT3 targetWaypoint = GetNextWaypoint();
    float distToWaypoint = DistanceXZ(m_currentPosition, targetWaypoint);

    const float waypointThreshold = m_radius * 0.5f;
    if (distToWaypoint <= waypointThreshold) {
        m_currentWaypointIndex++;
        
        if (m_currentWaypointIndex >= static_cast<int>(m_currentPath.waypoints.size())) {
            // パスの全ウェイポイントを通過したら到達とみなす
            Stop();
            m_hasPath = false;
            return;
        }
        
        targetWaypoint = GetNextWaypoint();
    }

    CalculateDesiredVelocity(deltaTime);
}

void NavMeshAgentComponent::CalculateDesiredVelocity(float deltaTime) {
    XMFLOAT3 steeringTarget = GetSteeringTarget();
    
    XMFLOAT3 direction;
    direction.x = steeringTarget.x - m_currentPosition.x;
    direction.y = 0;
    direction.z = steeringTarget.z - m_currentPosition.z;
    
    direction = Normalize(direction);
    m_steeringDirection = direction;
    
    if (Length(direction) > 0.001f) {
        m_targetRotationY = std::atan2(direction.x, direction.z);
    }

    float remainingDist = GetRemainingDistance();
    float targetSpeed = m_maxSpeed;

    float cornerAngle = CalculateCornerAngle(m_currentWaypointIndex);
    float cornerFactor = GetCornerSpeedFactor(cornerAngle);
    targetSpeed *= cornerFactor;

    if (m_autoBraking && remainingDist < m_stoppingDistance * 3.0f) {
        float brakeFactor = remainingDist / (m_stoppingDistance * 3.0f);
        targetSpeed = std::min(targetSpeed, m_maxSpeed * std::max(0.1f, brakeFactor));
    }

    if (m_currentSpeed < targetSpeed) {
        m_currentSpeed += m_acceleration * deltaTime;
        m_currentSpeed = std::min(m_currentSpeed, targetSpeed);
    } else if (m_currentSpeed > targetSpeed) {
        m_currentSpeed -= m_acceleration * 2.0f * deltaTime;
        m_currentSpeed = std::max(m_currentSpeed, targetSpeed);
    }

    m_desiredVelocity.x = direction.x * m_currentSpeed;
    m_desiredVelocity.y = 0;
    m_desiredVelocity.z = direction.z * m_currentSpeed;
    
    UpdateRotation(deltaTime);
}

void NavMeshAgentComponent::UpdateRotation(float deltaTime) {
    // 角速度（ラジアン/秒）に変換
    float angularSpeedRad = m_angularSpeed * (Math::PI / 180.0f);
    
    // 現在の回転から目標回転への差分
    float diff = AngleDifference(m_currentRotationY, m_targetRotationY);
    
    // 最大回転量
    float maxRotation = angularSpeedRad * deltaTime;
    
    // 回転量を制限
    if (std::abs(diff) <= maxRotation) {
        m_currentRotationY = m_targetRotationY;
    } else {
        float sign = (diff > 0.0f) ? 1.0f : -1.0f;
        m_currentRotationY += sign * maxRotation;
        m_currentRotationY = NormalizeAngle(m_currentRotationY);
    }
}

float NavMeshAgentComponent::NormalizeAngle(float angle) {
    // -π ～ π に正規化
    while (angle > Math::PI) angle -= Math::TWO_PI;
    while (angle < -Math::PI) angle += Math::TWO_PI;
    return angle;
}

float NavMeshAgentComponent::AngleDifference(float from, float to) {
    // 最短回転方向で角度差を計算
    float diff = NormalizeAngle(to - from);
    return diff;
}

XMFLOAT3 NavMeshAgentComponent::GetNextWaypoint() const {
    if (m_currentPath.waypoints.empty())
        return m_destination;

    if (m_currentWaypointIndex >= static_cast<int>(m_currentPath.waypoints.size()))
        return m_destination;

    return m_currentPath.waypoints[m_currentWaypointIndex];
}


float NavMeshAgentComponent::CalculateCornerAngle(int waypointIndex) const {
    if (m_currentPath.waypoints.size() < 2) return 0.0f;
    if (waypointIndex < 0 || waypointIndex >= static_cast<int>(m_currentPath.waypoints.size()) - 1) return 0.0f;
    
    XMFLOAT3 current = (waypointIndex == 0) ? m_currentPosition : m_currentPath.waypoints[waypointIndex - 1];
    XMFLOAT3 corner = m_currentPath.waypoints[waypointIndex];
    XMFLOAT3 next = m_currentPath.waypoints[waypointIndex + 1];
    
    XMFLOAT3 dir1 = {corner.x - current.x, 0, corner.z - current.z};
    XMFLOAT3 dir2 = {next.x - corner.x, 0, next.z - corner.z};
    
    dir1 = Normalize(dir1);
    dir2 = Normalize(dir2);
    
    float dot = dir1.x * dir2.x + dir1.z * dir2.z;
    dot = std::max(-1.0f, std::min(1.0f, dot));
    
    return std::acos(dot);
}

float NavMeshAgentComponent::GetCornerSpeedFactor(float angleRadians) const {
    constexpr float sharpTurnThreshold = Math::PI * 0.25f;
    constexpr float rightAngleThreshold = Math::PI * 0.5f;
    
    if (angleRadians < sharpTurnThreshold) return 1.0f;
    
    if (angleRadians >= rightAngleThreshold) return 0.3f;
    
    float t = (angleRadians - sharpTurnThreshold) / (rightAngleThreshold - sharpTurnThreshold);
    return 1.0f - t * 0.7f;
}

XMFLOAT3 NavMeshAgentComponent::GetSteeringTarget() const {
    if (m_currentPath.waypoints.empty()) return m_destination;
    
    constexpr float lookAheadDistance = 2.0f;
    float accumulated = 0.0f;
    
    int idx = m_currentWaypointIndex;
    XMFLOAT3 prevPoint = m_currentPosition;
    
    while (idx < static_cast<int>(m_currentPath.waypoints.size())) {
        const auto& wp = m_currentPath.waypoints[idx];
        float segmentDist = DistanceXZ(prevPoint, wp);
        
        if (accumulated + segmentDist >= lookAheadDistance) {
            float t = (lookAheadDistance - accumulated) / segmentDist;
            return {
                prevPoint.x + t * (wp.x - prevPoint.x),
                prevPoint.y + t * (wp.y - prevPoint.y),
                prevPoint.z + t * (wp.z - prevPoint.z)
            };
        }
        
        accumulated += segmentDist;
        prevPoint = wp;
        idx++;
    }
    
    return m_destination;
}

void NavMeshAgentComponent::DrawDebug(DebugRenderer* debugRenderer) const {
    if (!debugRenderer || !m_debugDrawEnabled)
        return;

    Vector3 center(m_currentPosition.x, m_currentPosition.y, m_currentPosition.z);
    
    // 色設定
    Vector4 radiusColor(0.0f, 0.8f, 1.0f, 0.8f);    // シアン
    Vector4 heightColor(1.0f, 0.8f, 0.0f, 0.6f);    // オレンジ
    Vector4 directionColor(0.0f, 1.0f, 0.0f, 1.0f); // 緑

    constexpr int segments = 24;
    const float angleStep = Math::TWO_PI / segments;

    // 底面の円（半径表示）- X-Ray描画でモデルに隠れない
    for (int i = 0; i < segments; ++i) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;

        Vector3 p1(
            center.GetX() + m_radius * std::cos(angle1),
            center.GetY() + 0.05f,
            center.GetZ() + m_radius * std::sin(angle1)
        );
        Vector3 p2(
            center.GetX() + m_radius * std::cos(angle2),
            center.GetY() + 0.05f,
            center.GetZ() + m_radius * std::sin(angle2)
        );
        debugRenderer->AddLineXRay(p1, p2, radiusColor);
    }

    // 上面の円
    for (int i = 0; i < segments; ++i) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;

        Vector3 p1(
            center.GetX() + m_radius * std::cos(angle1),
            center.GetY() + m_height,
            center.GetZ() + m_radius * std::sin(angle1)
        );
        Vector3 p2(
            center.GetX() + m_radius * std::cos(angle2),
            center.GetY() + m_height,
            center.GetZ() + m_radius * std::sin(angle2)
        );
        debugRenderer->AddLineXRay(p1, p2, heightColor);
    }

    // 縦の線（4本）
    for (int i = 0; i < 4; ++i) {
        float angle = i * Math::HALF_PI;
        Vector3 bottom(
            center.GetX() + m_radius * std::cos(angle),
            center.GetY() + 0.05f,
            center.GetZ() + m_radius * std::sin(angle)
        );
        Vector3 top(
            center.GetX() + m_radius * std::cos(angle),
            center.GetY() + m_height,
            center.GetZ() + m_radius * std::sin(angle)
        );
        debugRenderer->AddLineXRay(bottom, top, heightColor);
    }

    // 移動方向の矢印
    if (m_isMoving) {
        Vector3 dirStart = center;
        dirStart = Vector3(dirStart.GetX(), dirStart.GetY() + m_height * 0.5f, dirStart.GetZ());
        
        Vector3 dirEnd(
            dirStart.GetX() + m_steeringDirection.x * m_radius * 2.0f,
            dirStart.GetY(),
            dirStart.GetZ() + m_steeringDirection.z * m_radius * 2.0f
        );
        debugRenderer->AddLineXRay(dirStart, dirEnd, directionColor);

        // 矢じり
        float arrowSize = m_radius * 0.3f;
        float dirAngle = std::atan2(m_steeringDirection.z, m_steeringDirection.x);
        
        Vector3 arrow1(
            dirEnd.GetX() - arrowSize * std::cos(dirAngle - 0.5f),
            dirEnd.GetY(),
            dirEnd.GetZ() - arrowSize * std::sin(dirAngle - 0.5f)
        );
        Vector3 arrow2(
            dirEnd.GetX() - arrowSize * std::cos(dirAngle + 0.5f),
            dirEnd.GetY(),
            dirEnd.GetZ() - arrowSize * std::sin(dirAngle + 0.5f)
        );
        debugRenderer->AddLineXRay(dirEnd, arrow1, directionColor);
        debugRenderer->AddLineXRay(dirEnd, arrow2, directionColor);
    }
}

} // namespace UnoEngine
