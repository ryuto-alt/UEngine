#pragma once

#include "../Core/Component.h"
#include <DirectXMath.h>
#include <vector>

namespace UnoEngine {

/// NavMesh上を移動するエージェントコンポーネント
class NavAgentComponent : public Component {
public:
    enum class AgentState {
        Idle,       // 待機中
        Moving,     // 移動中
        Arrived     // 到着
    };

    NavAgentComponent() = default;
    ~NavAgentComponent() override = default;

    // Component lifecycle
    void Awake() override;
    void Start() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // ========== Navigation ==========
    /// 目的地を設定してパス計算開始
    bool SetDestination(const DirectX::XMFLOAT3& destination);
    
    /// 移動を停止
    void Stop();
    
    /// 現在のパスをクリア
    void ClearPath();
    
    /// 目的地に到達したか
    bool HasReachedDestination() const;

    // ========== Properties ==========
    float GetSpeed() const { return speed_; }
    void SetSpeed(float speed) { speed_ = speed; }

    float GetAngularSpeed() const { return angularSpeed_; }
    void SetAngularSpeed(float angularSpeed) { angularSpeed_ = angularSpeed; }

    float GetAcceleration() const { return acceleration_; }
    void SetAcceleration(float acceleration) { acceleration_ = acceleration; }

    float GetStoppingDistance() const { return stoppingDistance_; }
    void SetStoppingDistance(float distance) { stoppingDistance_ = distance; }

    float GetBaseOffset() const { return baseOffset_; }
    void SetBaseOffset(float offset) { baseOffset_ = offset; }

    bool IsAutobrake() const { return autoBrake_; }
    void SetAutobrake(bool autoBrake) { autoBrake_ = autoBrake; }

    // ========== State ==========
    AgentState GetState() const { return state_; }
    const DirectX::XMFLOAT3& GetDestination() const { return destination_; }
    const DirectX::XMFLOAT3& GetVelocity() const { return velocity_; }
    float GetRemainingDistance() const { return remainingDistance_; }
    bool HasPath() const { return !currentPath_.empty(); }

    // ========== Debug ==========
    bool IsPathVisualized() const { return visualizePath_; }
    void SetPathVisualized(bool visualize) { visualizePath_ = visualize; }
    const std::vector<DirectX::XMFLOAT3>& GetCurrentPath() const { return currentPath_; }

private:
    void UpdateMovement(float deltaTime);
    void UpdateRotation(float deltaTime);
    bool CalculatePath();
    DirectX::XMFLOAT3 GetNextWaypoint() const;

    // Movement parameters
    float speed_ = 3.5f;              // 移動速度 (m/s)
    float angularSpeed_ = 360.0f;     // 回転速度 (deg/s)
    float acceleration_ = 8.0f;       // 加速度 (m/s^2)
    float stoppingDistance_ = 0.1f;   // 停止距離 (m)
    float baseOffset_ = 0.0f;         // 地面からのオフセット
    bool autoBrake_ = true;           // 到着時に自動減速

    // Navigation state
    AgentState state_ = AgentState::Idle;
    DirectX::XMFLOAT3 destination_ = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 velocity_ = {0.0f, 0.0f, 0.0f};
    std::vector<DirectX::XMFLOAT3> currentPath_;
    size_t currentWaypointIndex_ = 0;
    float remainingDistance_ = 0.0f;
    float currentSpeed_ = 0.0f;

    // Debug
    bool visualizePath_ = false;
};

} // namespace UnoEngine
