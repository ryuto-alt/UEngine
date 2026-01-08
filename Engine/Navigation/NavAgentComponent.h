#pragma once

#include "../Core/Component.h"
#include <DirectXMath.h>
#include <vector>
#include <functional>

namespace UnoEngine {

/// NavMesh上を移動するエージェントコンポーネント
/// DetourCrowdを使用して複数エージェントの衝突回避と滑らかな移動を実現
class NavAgentComponent : public Component {
public:
    enum class AgentState {
        Idle,       // 待機中
        Moving,     // 移動中
        Arrived,    // 到着
        Wandering,  // 徘徊中
        Patrolling, // パトロール中
        Chasing     // 追跡中
    };

    /// 徘徊モード
    enum class WanderMode {
        Random,         // 完全ランダム
        AroundSpawn,    // スポーン地点周辺
        AroundCurrent   // 現在位置周辺
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

    // ========== Wander (徘徊) ==========
    /// 徘徊を開始
    void StartWander(WanderMode mode = WanderMode::AroundSpawn, float radius = 10.0f);
    
    /// 徘徊を停止
    void StopWander();
    
    /// 徘徊中かどうか
    bool IsWandering() const { return state_ == AgentState::Wandering; }
    
    /// 次のランダム目的地を選択（内部用、Luaからも呼べる）
    bool PickRandomDestination();

    // ========== Patrol (パトロール) ==========
    /// パトロールポイントを設定して開始
    void StartPatrol(const std::vector<DirectX::XMFLOAT3>& points, bool loop = true);
    
    /// パトロールを停止
    void StopPatrol();
    
    /// パトロール中かどうか
    bool IsPatrolling() const { return state_ == AgentState::Patrolling; }
    
    /// パトロールポイントを追加
    void AddPatrolPoint(const DirectX::XMFLOAT3& point);
    
    /// パトロールポイントをクリア
    void ClearPatrolPoints();

    // ========== Chase (追跡) ==========
    /// ターゲット追跡を開始
    void StartChase(GameObject* target, float updateInterval = 0.5f);
    
    /// 追跡を停止
    void StopChase();
    
    /// 追跡中かどうか
    bool IsChasing() const { return state_ == AgentState::Chasing; }

    // ========== Properties ==========
    float GetSpeed() const { return speed_; }
    void SetSpeed(float speed);

    float GetAngularSpeed() const { return angularSpeed_; }
    void SetAngularSpeed(float angularSpeed) { angularSpeed_ = angularSpeed; }

    float GetAcceleration() const { return acceleration_; }
    void SetAcceleration(float acceleration);

    float GetStoppingDistance() const { return stoppingDistance_; }
    void SetStoppingDistance(float distance) { stoppingDistance_ = distance; }

    float GetBaseOffset() const { return baseOffset_; }
    void SetBaseOffset(float offset) { baseOffset_ = offset; }

    bool IsAutobrake() const { return autoBrake_; }
    void SetAutobrake(bool autoBrake) { autoBrake_ = autoBrake; }
    
    float GetAgentRadius() const { return agentRadius_; }
    void SetAgentRadius(float radius) { agentRadius_ = radius; }
    
    float GetAgentHeight() const { return agentHeight_; }
    void SetAgentHeight(float height) { agentHeight_ = height; }
    
    /// Crowdシステムを使用するかどうか
    bool IsUsingCrowd() const { return useCrowd_; }
    void SetUseCrowd(bool use) { useCrowd_ = use; }

    /// 待機時間（徘徊/パトロール時の目的地到達後の待機秒数）
    float GetWaitTime() const { return waitTime_; }
    void SetWaitTime(float time) { waitTime_ = time; }

    /// 直進モード（Crowdの回避ロジックをバイパス）
    bool IsDirectMoveEnabled() const { return directMoveEnabled_; }
    void SetDirectMoveEnabled(bool enabled) { directMoveEnabled_ = enabled; }

    /// 初期向きを設定（ラジアン、Y軸回転）
    void SetInitialYaw(float yaw) { initialYaw_ = yaw; }
    float GetInitialYaw() const { return initialYaw_; }

    // ========== State ==========
    AgentState GetState() const { return state_; }
    const DirectX::XMFLOAT3& GetDestination() const { return destination_; }
    DirectX::XMFLOAT3 GetVelocity() const;
    float GetRemainingDistance() const;
    bool HasPath() const { return !currentPath_.empty() || crowdAgentIndex_ >= 0; }
    
    /// CrowdエージェントIDを取得
    int GetCrowdAgentIndex() const { return crowdAgentIndex_; }

    // ========== Debug ==========
    bool IsPathVisualized() const { return visualizePath_; }
    void SetPathVisualized(bool visualize) { visualizePath_ = visualize; }
    const std::vector<DirectX::XMFLOAT3>& GetCurrentPath() const { return currentPath_; }

    // ========== Events ==========
    using DestinationReachedCallback = std::function<void()>;
    void SetOnDestinationReached(DestinationReachedCallback callback) { onDestinationReached_ = std::move(callback); }

private:
    void InitializeCrowdAgent();
    void UpdateCrowdAgent();
    void UpdateDirectMove();  // 直進モードの速度制御
    void UpdateWander(float deltaTime);
    void UpdatePatrol(float deltaTime);
    void UpdateChase(float deltaTime);
    void UpdateRotation(float deltaTime);
    void SyncTransformFromCrowd();
    bool CalculatePath();
    DirectX::XMFLOAT3 GetNextWaypoint() const;

    // Movement parameters
    float speed_ = 3.5f;              // 移動速度 (m/s)
    float angularSpeed_ = 360.0f;     // 回転速度 (deg/s)
    float acceleration_ = 8.0f;       // 加速度 (m/s^2)
    float stoppingDistance_ = 0.5f;   // 停止距離 (m)
    float baseOffset_ = 0.0f;         // 地面からのオフセット
    bool autoBrake_ = true;           // 到着時に自動減速
    
    // Agent physical properties
    float agentRadius_ = 0.4f;        // エージェント半径
    float agentHeight_ = 1.8f;        // エージェント高さ

    // Navigation state
    AgentState state_ = AgentState::Idle;
    DirectX::XMFLOAT3 destination_ = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 velocity_ = {0.0f, 0.0f, 0.0f};
    std::vector<DirectX::XMFLOAT3> currentPath_;
    size_t currentWaypointIndex_ = 0;
    float remainingDistance_ = 0.0f;
    float currentSpeed_ = 0.0f;
    
    // Crowd integration
    int crowdAgentIndex_ = -1;        // DetourCrowdのエージェントインデックス
    bool useCrowd_ = true;            // Crowdシステムを使用するか
    bool directMoveEnabled_ = false;  // 直進モード（デフォルトOFF、Crowd本来の動作を使用）
    float initialYaw_ = 0.0f;         // 初期向き（ラジアン）
    
    // Wander state
    WanderMode wanderMode_ = WanderMode::AroundSpawn;
    float wanderRadius_ = 10.0f;
    DirectX::XMFLOAT3 spawnPosition_ = {0.0f, 0.0f, 0.0f};
    float waitTime_ = 1.0f;           // 到着後の待機時間
    float currentWaitTime_ = 0.0f;
    bool isWaiting_ = false;
    bool hasInitialDestination_ = false;
    
    // Patrol state
    std::vector<DirectX::XMFLOAT3> patrolPoints_;
    size_t currentPatrolIndex_ = 0;
    bool patrolLoop_ = true;
    bool patrolReverse_ = false;      // ループしない場合の折り返し用
    
    // Chase state
    GameObject* chaseTarget_ = nullptr;
    float chaseUpdateInterval_ = 0.5f;
    float chaseUpdateTimer_ = 0.0f;

    // Debug
    bool visualizePath_ = false;

    // Smoothing state
    float smoothedYaw_ = 0.0f;        // スムーズ化されたYaw角度
    bool yawInitialized_ = false;     // 初期Yaw設定済みフラグ

    // Events
    DestinationReachedCallback onDestinationReached_;
};

} // namespace UnoEngine
