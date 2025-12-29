#pragma once

#include "../../Core/Component.h"
#include "NavMeshTypes.h"
#include <DirectXMath.h>

namespace UnoEngine {

// NavMeshエージェントコンポーネント
// パス計算と「意図」の出力を担当。実際の移動は別コンポーネントが行う。
class NavMeshAgentComponent : public Component {
public:
    NavMeshAgentComponent() = default;
    ~NavMeshAgentComponent() override = default;

    void Awake() override;
    void OnUpdate(float deltaTime) override;

    bool SetDestination(const DirectX::XMFLOAT3& target);
    void Stop();
    void ResetPath();

    bool HasPath() const { return m_hasPath; }
    bool IsMoving() const { return m_isMoving; }
    bool HasReachedDestination() const;
    float GetRemainingDistance() const;

    const DirectX::XMFLOAT3& GetDesiredVelocity() const { return m_desiredVelocity; }
    const DirectX::XMFLOAT3& GetSteeringDirection() const { return m_steeringDirection; }
    float GetCurrentSpeed() const { return m_currentSpeed; }
    float GetCurrentRotationY() const { return m_currentRotationY; }
    float GetTargetRotationY() const { return m_targetRotationY; }

    void SyncPosition(const DirectX::XMFLOAT3& worldPosition);
    bool SnapToNavMesh();

    void SetAgentTypeId(uint32_t id) { m_agentTypeId = id; }
    uint32_t GetAgentTypeId() const { return m_agentTypeId; }

    void SetRadius(float radius) { m_radius = radius; }
    float GetRadius() const { return m_radius; }

    void SetHeight(float height) { m_height = height; }
    float GetHeight() const { return m_height; }

    void SetMaxSpeed(float speed) { m_maxSpeed = speed; }
    float GetMaxSpeed() const { return m_maxSpeed; }

    void SetAcceleration(float accel) { m_acceleration = accel; }
    float GetAcceleration() const { return m_acceleration; }

    void SetAngularSpeed(float speed) { m_angularSpeed = speed; }
    float GetAngularSpeed() const { return m_angularSpeed; }

    void SetStoppingDistance(float dist) { m_stoppingDistance = dist; }
    float GetStoppingDistance() const { return m_stoppingDistance; }

    void SetAutoBraking(bool enabled) { m_autoBraking = enabled; }
    bool GetAutoBraking() const { return m_autoBraking; }
    
    void SetUpdateTransform(bool enabled) { m_updateTransform = enabled; }
    bool GetUpdateTransform() const { return m_updateTransform; }

    void SetAutoRepath(bool enabled) { m_autoRepath = enabled; }
    bool GetAutoRepath() const { return m_autoRepath; }

    void SetSpeed(float speed) { m_maxSpeed = speed; }
    float GetSpeed() const { return m_maxSpeed; }

    const NavMeshPath& GetCurrentPath() const { return m_currentPath; }
    int GetCurrentWaypointIndex() const { return m_currentWaypointIndex; }

    void DrawDebug(class DebugRenderer* debugRenderer) const;
    void SetDebugDrawEnabled(bool enabled) { m_debugDrawEnabled = enabled; }
    bool IsDebugDrawEnabled() const { return m_debugDrawEnabled; }

private:
    void UpdatePathFollowing(float deltaTime);
    void CalculateDesiredVelocity(float deltaTime);
    void UpdateRotation(float deltaTime);
    DirectX::XMFLOAT3 GetNextWaypoint() const;
    
    float CalculateCornerAngle(int waypointIndex) const;
    float GetCornerSpeedFactor(float angleRadians) const;
    DirectX::XMFLOAT3 GetSteeringTarget() const;
    
    static float NormalizeAngle(float angle);
    static float AngleDifference(float from, float to);

    uint32_t m_agentTypeId = 0;
    float m_radius = 0.5f;
    float m_height = 2.0f;

    float m_maxSpeed = 3.5f;
    float m_acceleration = 8.0f;
    float m_angularSpeed = 120.0f;
    float m_stoppingDistance = 0.1f;
    bool m_autoBraking = true;
    bool m_updateTransform = true;  // Transform自動更新（デフォルトON）
    bool m_autoRepath = true;

    NavMeshPath m_currentPath;
    DirectX::XMFLOAT3 m_currentPosition = {0, 0, 0};
    DirectX::XMFLOAT3 m_destination = {0, 0, 0};
    int m_currentWaypointIndex = 0;
    bool m_hasPath = false;
    bool m_isMoving = false;
    bool m_positionSynced = false;

    DirectX::XMFLOAT3 m_desiredVelocity = {0, 0, 0};
    DirectX::XMFLOAT3 m_steeringDirection = {0, 0, 1};
    float m_currentSpeed = 0.0f;
    float m_currentRotationY = 0.0f;
    float m_targetRotationY = 0.0f;

    bool m_debugDrawEnabled = false;
};

} // namespace UnoEngine
