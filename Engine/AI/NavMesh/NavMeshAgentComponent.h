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

    // Lifecycle
    void Awake() override;
    void OnUpdate(float deltaTime) override;

    // === 目的地設定 ===
    bool SetDestination(const DirectX::XMFLOAT3& target);
    void Stop();
    void ResetPath();

    // === 状態取得 ===
    bool HasPath() const { return m_hasPath; }
    bool IsMoving() const { return m_isMoving; }
    bool HasReachedDestination() const;
    float GetRemainingDistance() const;

    // === 出力取得（移動コンポーネント用） ===
    const DirectX::XMFLOAT3& GetDesiredVelocity() const { return m_desiredVelocity; }
    const DirectX::XMFLOAT3& GetSteeringDirection() const { return m_steeringDirection; }
    float GetCurrentSpeed() const { return m_currentSpeed; }

    // === フィードバックループ ===
    // 物理移動後の実際の位置を同期（毎フレーム呼び出し推奨）
    void SyncPosition(const DirectX::XMFLOAT3& worldPosition);

    // NavMesh上に位置を補正
    bool SnapToNavMesh();

    // === エージェント設定 ===
    void SetAgentTypeId(uint32_t id) { m_agentTypeId = id; }
    uint32_t GetAgentTypeId() const { return m_agentTypeId; }

    void SetRadius(float radius) { m_radius = radius; }
    float GetRadius() const { return m_radius; }

    void SetHeight(float height) { m_height = height; }
    float GetHeight() const { return m_height; }

    // === 移動パラメータ ===
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

    // === デバッグ ===
    const NavMeshPath& GetCurrentPath() const { return m_currentPath; }
    int GetCurrentWaypointIndex() const { return m_currentWaypointIndex; }

private:
    void UpdatePathFollowing(float deltaTime);
    void CalculateDesiredVelocity(float deltaTime);
    DirectX::XMFLOAT3 GetNextWaypoint() const;

    // === エージェント設定 ===
    uint32_t m_agentTypeId = 0;
    float m_radius = 0.5f;
    float m_height = 2.0f;

    // === 移動パラメータ ===
    float m_maxSpeed = 3.5f;
    float m_acceleration = 8.0f;
    float m_angularSpeed = 120.0f;  // degrees/sec
    float m_stoppingDistance = 0.1f;
    bool m_autoBraking = true;

    // === 内部状態 ===
    NavMeshPath m_currentPath;
    DirectX::XMFLOAT3 m_currentPosition = {0, 0, 0};
    DirectX::XMFLOAT3 m_destination = {0, 0, 0};
    int m_currentWaypointIndex = 0;
    bool m_hasPath = false;
    bool m_isMoving = false;
    bool m_positionSynced = false;

    // === 出力 ===
    DirectX::XMFLOAT3 m_desiredVelocity = {0, 0, 0};
    DirectX::XMFLOAT3 m_steeringDirection = {0, 0, 1};
    float m_currentSpeed = 0.0f;
};

} // namespace UnoEngine
