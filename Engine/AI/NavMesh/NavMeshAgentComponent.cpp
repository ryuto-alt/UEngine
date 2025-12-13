#include "pch.h"
#include "NavMeshAgentComponent.h"
#include "NavMeshSystem.h"
#include "../../Core/GameObject.h"
#include "../../Core/Transform.h"
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
        }
    }

    UpdatePathFollowing(deltaTime);
    m_positionSynced = false;  // 次フレームで再同期が必要
}

bool NavMeshAgentComponent::SetDestination(const XMFLOAT3& target) {
    auto& navSystem = NavMeshSystem::GetInstance();
    
    if (!navSystem.HasNavMesh())
        return false;

    // パス検索
    m_currentPath = navSystem.FindPath(m_currentPosition, target);
    
    if (!m_currentPath.IsValid())
        return false;

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

    // 目的地到達チェック
    if (HasReachedDestination()) {
        Stop();
        return;
    }

    // 現在のウェイポイントへの距離チェック
    XMFLOAT3 targetWaypoint = GetNextWaypoint();
    float distToWaypoint = DistanceXZ(m_currentPosition, targetWaypoint);

    // ウェイポイント到達判定（半径ベース）
    const float waypointThreshold = m_radius * 0.5f;
    if (distToWaypoint <= waypointThreshold) {
        m_currentWaypointIndex++;
        
        // 最終ウェイポイント到達
        if (m_currentWaypointIndex >= static_cast<int>(m_currentPath.waypoints.size())) {
            if (HasReachedDestination()) {
                Stop();
                return;
            }
            // パスは終わったが目的地に未到達 → 再検索
            SetDestination(m_destination);
            return;
        }
        
        targetWaypoint = GetNextWaypoint();
    }

    CalculateDesiredVelocity(deltaTime);
}

void NavMeshAgentComponent::CalculateDesiredVelocity(float deltaTime) {
    XMFLOAT3 targetWaypoint = GetNextWaypoint();
    
    // 移動方向を計算（XZ平面）
    XMFLOAT3 direction;
    direction.x = targetWaypoint.x - m_currentPosition.x;
    direction.y = 0;  // 水平移動のみ
    direction.z = targetWaypoint.z - m_currentPosition.z;
    
    direction = Normalize(direction);
    m_steeringDirection = direction;

    // 速度計算
    float remainingDist = GetRemainingDistance();
    float targetSpeed = m_maxSpeed;

    // 自動ブレーキ
    if (m_autoBraking && remainingDist < m_stoppingDistance * 3.0f) {
        float brakeFactor = remainingDist / (m_stoppingDistance * 3.0f);
        targetSpeed = m_maxSpeed * std::max(0.1f, brakeFactor);
    }

    // 加速/減速
    if (m_currentSpeed < targetSpeed) {
        m_currentSpeed += m_acceleration * deltaTime;
        m_currentSpeed = std::min(m_currentSpeed, targetSpeed);
    } else if (m_currentSpeed > targetSpeed) {
        m_currentSpeed -= m_acceleration * deltaTime;
        m_currentSpeed = std::max(m_currentSpeed, targetSpeed);
    }

    // 希望速度ベクトル
    m_desiredVelocity.x = direction.x * m_currentSpeed;
    m_desiredVelocity.y = 0;
    m_desiredVelocity.z = direction.z * m_currentSpeed;
}

XMFLOAT3 NavMeshAgentComponent::GetNextWaypoint() const {
    if (m_currentPath.waypoints.empty())
        return m_destination;

    if (m_currentWaypointIndex >= static_cast<int>(m_currentPath.waypoints.size()))
        return m_destination;

    return m_currentPath.waypoints[m_currentWaypointIndex];
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

    // 底面の円（半径表示）
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
        debugRenderer->AddLine(p1, p2, radiusColor);
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
        debugRenderer->AddLine(p1, p2, heightColor);
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
        debugRenderer->AddLine(bottom, top, heightColor);
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
        debugRenderer->AddLine(dirStart, dirEnd, directionColor);

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
        debugRenderer->AddLine(dirEnd, arrow1, directionColor);
        debugRenderer->AddLine(dirEnd, arrow2, directionColor);
    }
}

} // namespace UnoEngine
