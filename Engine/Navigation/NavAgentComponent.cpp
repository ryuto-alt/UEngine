#include "pch.h"
#include "NavAgentComponent.h"
#include "NavMeshManager.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"
#include "../Core/Logger.h"
#include <cmath>

namespace UnoEngine {

void NavAgentComponent::Awake() {
    state_ = AgentState::Idle;
    currentPath_.clear();
    velocity_ = {0.0f, 0.0f, 0.0f};
    yawInitialized_ = false;
    smoothedYaw_ = 0.0f;

    if (gameObject_) {
        auto pos = gameObject_->GetTransform().GetPosition();
        spawnPosition_ = {pos.GetX(), pos.GetY(), pos.GetZ()};
    }
}

void NavAgentComponent::Start() {
    // 初期化はOnUpdateで遅延実行（Luaスクリプトが設定を行う時間を確保）
    // InitializeCrowdAgent() は OnUpdate で呼ばれる
}

void NavAgentComponent::OnUpdate(float deltaTime) {
    if (!enabled_) {
        return;
    }

    auto& navMesh = Navigation::NavMeshManager::Get();
    
    // NavMeshビルド中は何もしない（スレッドセーフティ）
    if (navMesh.IsBuilding()) {
        return;
    }
    
    // Crowdが破棄された場合（NavMesh再ベイク等）、エージェントをリセット
    if (crowdAgentIndex_ >= 0 && !navMesh.IsCrowdInitialized()) {
        Logger::Info("[NavAgent] Crowd was reset, re-initializing agent...");
        crowdAgentIndex_ = -1;
        hasInitialDestination_ = false;
    }

    // エージェントがまだ初期化されていなければ初期化を試みる
    if (crowdAgentIndex_ < 0 && useCrowd_) {
        InitializeCrowdAgent();
    }

    // 状態別更新
    switch (state_) {
        case AgentState::Idle:
            break;
            
        case AgentState::Moving:
            UpdateCrowdAgent();
            break;
            
        case AgentState::Wandering:
            UpdateWander(deltaTime);
            break;
            
        case AgentState::Patrolling:
            UpdatePatrol(deltaTime);
            break;
            
        case AgentState::Chasing:
            UpdateChase(deltaTime);
            break;
            
        case AgentState::Arrived:
            break;
    }

    // 位置を同期
    if (crowdAgentIndex_ >= 0) {
        SyncTransformFromCrowd();
    }
    
    UpdateRotation(deltaTime);
}

void NavAgentComponent::OnDestroy() {
    auto& navMesh = Navigation::NavMeshManager::Get();
    if (crowdAgentIndex_ >= 0) {
        navMesh.RemoveCrowdAgent(crowdAgentIndex_);
        crowdAgentIndex_ = -1;
    }
    ClearPath();
}

void NavAgentComponent::InitializeCrowdAgent() {
    Logger::Info("[NavAgent] InitializeCrowdAgent called - useCrowd={}, gameObject={}",
        useCrowd_, gameObject_ != nullptr);

    if (!useCrowd_ || !gameObject_) {
        return;
    }

    auto& navMesh = Navigation::NavMeshManager::Get();

    Logger::Info("[NavAgent] NavMesh state: IsBuilt={}, IsCrowdInitialized={}",
        navMesh.IsBuilt(), navMesh.IsCrowdInitialized());

    if (!navMesh.IsBuilt()) {
        Logger::Debug("[NavAgent] NavMesh not built yet, waiting...");
        return;
    }

    // Crowdがまだ初期化されていなければ初期化
    if (!navMesh.IsCrowdInitialized()) {
        Logger::Info("[NavAgent] Initializing crowd system...");
        if (!navMesh.InitializeCrowd(128, agentRadius_)) {
            Logger::Warning("[NavAgent] Failed to initialize crowd system");
            return;
        }
        Logger::Info("[NavAgent] Crowd system initialized successfully");
    }

    auto pos = gameObject_->GetTransform().GetPosition();
    DirectX::XMFLOAT3 position = {pos.GetX(), pos.GetY(), pos.GetZ()};

    // スポーン位置を記録
    spawnPosition_ = position;

    crowdAgentIndex_ = navMesh.AddCrowdAgent(position, agentRadius_, agentHeight_, speed_, acceleration_);

    if (crowdAgentIndex_ >= 0) {
        Logger::Info("[NavAgent] Agent {} added at ({:.2f}, {:.2f}, {:.2f})",
            crowdAgentIndex_, position.x, position.y, position.z);
    } else {
        Logger::Warning("[NavAgent] Failed to add crowd agent");
    }
}

bool NavAgentComponent::SetDestination(const DirectX::XMFLOAT3& destination) {
    destination_ = destination;
    
    auto& navMesh = Navigation::NavMeshManager::Get();
    
    if (crowdAgentIndex_ >= 0 && navMesh.IsCrowdInitialized()) {
        if (navMesh.SetAgentTarget(crowdAgentIndex_, destination)) {
            state_ = AgentState::Moving;
            isWaiting_ = false;
            return true;
        }
        return false;
    }
    
    // フォールバック: 従来のパス計算
    if (!CalculatePath()) {
        state_ = AgentState::Idle;
        return false;
    }

    state_ = AgentState::Moving;
    currentWaypointIndex_ = 0;
    return true;
}

void NavAgentComponent::Stop() {
    auto& navMesh = Navigation::NavMeshManager::Get();
    
    if (crowdAgentIndex_ >= 0) {
        navMesh.StopAgent(crowdAgentIndex_);
    }
    
    state_ = AgentState::Idle;
    velocity_ = {0.0f, 0.0f, 0.0f};
    currentSpeed_ = 0.0f;
    chaseTarget_ = nullptr;
}

void NavAgentComponent::ClearPath() {
    currentPath_.clear();
    currentWaypointIndex_ = 0;
    remainingDistance_ = 0.0f;
    Stop();
}

bool NavAgentComponent::HasReachedDestination() const {
    if (crowdAgentIndex_ >= 0) {
        auto& navMesh = Navigation::NavMeshManager::Get();
        return navMesh.HasAgentReachedTarget(crowdAgentIndex_, stoppingDistance_);
    }
    return state_ == AgentState::Arrived;
}

// ========== Wander ==========
void NavAgentComponent::StartWander(WanderMode mode, float radius) {
    Logger::Info("[NavAgent] StartWander called - mode: {}, radius: {:.1f}", 
        static_cast<int>(mode), radius);
    
    wanderMode_ = mode;
    wanderRadius_ = radius;
    
    if (mode == WanderMode::AroundSpawn && gameObject_) {
        auto pos = gameObject_->GetTransform().GetPosition();
        spawnPosition_ = {pos.GetX(), pos.GetY(), pos.GetZ()};
    }
    
    state_ = AgentState::Wandering;
    isWaiting_ = false;
    currentWaitTime_ = 0.0f;
    hasInitialDestination_ = false;  // Reset for new wander session
    
    // エージェントが初期化されるまで待機（UpdateWanderで処理）
    if (crowdAgentIndex_ < 0) {
        Logger::Info("[NavAgent] Agent not initialized yet, will pick destination when ready");
        return;
    }
    
    // エージェントが既に初期化されている場合は即座に目的地を設定
    hasInitialDestination_ = true;
    PickRandomDestination();
}

void NavAgentComponent::StopWander() {
    if (state_ == AgentState::Wandering) {
        Stop();
    }
}

bool NavAgentComponent::PickRandomDestination() {
    auto& navMesh = Navigation::NavMeshManager::Get();
    
    if (crowdAgentIndex_ < 0) {
        Logger::Warning("[NavAgent] Cannot pick destination - agent not initialized");
        return false;
    }
    
    DirectX::XMFLOAT3 newDest;
    bool found = false;
    
    switch (wanderMode_) {
        case WanderMode::Random:
            found = navMesh.GetRandomPointOnNavMesh(newDest);
            break;
            
        case WanderMode::AroundSpawn:
            found = navMesh.GetRandomPointAroundCircle(spawnPosition_, wanderRadius_, newDest);
            break;
            
        case WanderMode::AroundCurrent:
            if (gameObject_) {
                auto pos = gameObject_->GetTransform().GetPosition();
                DirectX::XMFLOAT3 current = {pos.GetX(), pos.GetY(), pos.GetZ()};
                found = navMesh.GetRandomPointAroundCircle(current, wanderRadius_, newDest);
            }
            break;
    }
    
    if (found) {
        Logger::Info("[NavAgent] Picked random destination: ({:.2f}, {:.2f}, {:.2f})", 
            newDest.x, newDest.y, newDest.z);
        destination_ = newDest;
        
        if (navMesh.SetAgentTarget(crowdAgentIndex_, newDest)) {
            return true;
        }
        Logger::Warning("[NavAgent] Failed to set agent target");
        return false;
    }
    
    Logger::Warning("[NavAgent] Failed to find random point on NavMesh");
    return false;
}

// ========== Patrol ==========
void NavAgentComponent::StartPatrol(const std::vector<DirectX::XMFLOAT3>& points, bool loop) {
    patrolPoints_ = points;
    patrolLoop_ = loop;
    patrolReverse_ = false;
    currentPatrolIndex_ = 0;
    
    if (patrolPoints_.empty()) {
        state_ = AgentState::Idle;
        return;
    }
    
    state_ = AgentState::Patrolling;
    isWaiting_ = false;
    currentWaitTime_ = 0.0f;
    
    SetDestination(patrolPoints_[0]);
}

void NavAgentComponent::StopPatrol() {
    if (state_ == AgentState::Patrolling) {
        Stop();
    }
}

void NavAgentComponent::AddPatrolPoint(const DirectX::XMFLOAT3& point) {
    patrolPoints_.push_back(point);
}

void NavAgentComponent::ClearPatrolPoints() {
    patrolPoints_.clear();
    currentPatrolIndex_ = 0;
}

// ========== Chase ==========
void NavAgentComponent::StartChase(GameObject* target, float updateInterval) {
    if (!target) {
        return;
    }
    
    chaseTarget_ = target;
    chaseUpdateInterval_ = updateInterval;
    chaseUpdateTimer_ = 0.0f;
    state_ = AgentState::Chasing;
    
    // 初回ターゲット設定
    auto targetPos = target->GetTransform().GetPosition();
    SetDestination({targetPos.GetX(), targetPos.GetY(), targetPos.GetZ()});
}

void NavAgentComponent::StopChase() {
    chaseTarget_ = nullptr;
    if (state_ == AgentState::Chasing) {
        Stop();
    }
}

// ========== Properties ==========
void NavAgentComponent::SetSpeed(float speed) {
    speed_ = speed;
    if (crowdAgentIndex_ >= 0) {
        auto& navMesh = Navigation::NavMeshManager::Get();
        navMesh.UpdateAgentParameters(crowdAgentIndex_, speed_, acceleration_);
    }
}

void NavAgentComponent::SetAcceleration(float acceleration) {
    acceleration_ = acceleration;
    if (crowdAgentIndex_ >= 0) {
        auto& navMesh = Navigation::NavMeshManager::Get();
        navMesh.UpdateAgentParameters(crowdAgentIndex_, speed_, acceleration_);
    }
}

DirectX::XMFLOAT3 NavAgentComponent::GetVelocity() const {
    if (crowdAgentIndex_ >= 0) {
        auto& navMesh = Navigation::NavMeshManager::Get();
        return navMesh.GetAgentVelocity(crowdAgentIndex_);
    }
    return velocity_;
}

float NavAgentComponent::GetRemainingDistance() const {
    if (crowdAgentIndex_ >= 0 && gameObject_) {
        auto pos = gameObject_->GetTransform().GetPosition();
        float dx = destination_.x - pos.GetX();
        float dy = destination_.y - pos.GetY();
        float dz = destination_.z - pos.GetZ();
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return remainingDistance_;
}

// ========== Private Update Methods ==========
void NavAgentComponent::UpdateCrowdAgent() {
    if (crowdAgentIndex_ < 0) {
        return;
    }

    auto& navMesh = Navigation::NavMeshManager::Get();

    // 直進モードで速度をオーバーライド
    if (directMoveEnabled_) {
        UpdateDirectMove();
    }

    if (navMesh.HasAgentReachedTarget(crowdAgentIndex_, stoppingDistance_)) {
        state_ = AgentState::Arrived;

        if (onDestinationReached_) {
            onDestinationReached_();
        }
    }
}

void NavAgentComponent::UpdateDirectMove() {
    if (crowdAgentIndex_ < 0) {
        return;
    }

    auto& navMesh = Navigation::NavMeshManager::Get();

    DirectX::XMFLOAT3 nextCorner;
    float distToCorner = 0.0f;

    if (!navMesh.GetNextCorner(crowdAgentIndex_, nextCorner, distToCorner)) {
        return;
    }

    // 次のコーナーへの方向ベクトルを算出
    auto agentPos = navMesh.GetAgentPosition(crowdAgentIndex_);
    float dx = nextCorner.x - agentPos.x;
    float dz = nextCorner.z - agentPos.z;
    float length = std::sqrt(dx * dx + dz * dz);

    if (length < 0.001f) {
        return;
    }

    // 正規化して速度に変換
    float invLen = 1.0f / length;
    DirectX::XMFLOAT3 desiredVel = {
        dx * invLen * speed_,
        0.0f,
        dz * invLen * speed_
    };

    // 速度をオーバーライド（純粋なパス追従）
    navMesh.OverrideAgentVelocity(crowdAgentIndex_, desiredVel);
}

void NavAgentComponent::UpdateWander(float deltaTime) {
    if (crowdAgentIndex_ < 0) {
        return;
    }

    auto& navMesh = Navigation::NavMeshManager::Get();

    // 目的地がまだ設定されていない場合（初期化直後）
    if (!hasInitialDestination_) {
        hasInitialDestination_ = true;
        PickRandomDestination();
        return;
    }

    // 待機モードの処理
    if (isWaiting_) {
        // waitTime <= 0 なら即座に次へ
        if (waitTime_ <= 0.0f) {
            isWaiting_ = false;
            PickRandomDestination();
            return;
        }

        currentWaitTime_ += deltaTime;
        if (currentWaitTime_ >= waitTime_) {
            isWaiting_ = false;
            currentWaitTime_ = 0.0f;
            PickRandomDestination();
        }
        return;
    }

    // 直進モードで速度オーバーライド
    if (directMoveEnabled_) {
        UpdateDirectMove();
    }

    // 到着判定
    if (navMesh.HasAgentReachedTarget(crowdAgentIndex_, stoppingDistance_)) {
        // waitTime <= 0 なら待機せず即座に次の目的地へ
        if (waitTime_ <= 0.0f) {
            PickRandomDestination();
        } else {
            isWaiting_ = true;
            currentWaitTime_ = 0.0f;
        }
    }
}

void NavAgentComponent::UpdatePatrol(float deltaTime) {
    if (patrolPoints_.empty()) {
        state_ = AgentState::Idle;
        return;
    }

    if (isWaiting_) {
        currentWaitTime_ += deltaTime;
        if (currentWaitTime_ >= waitTime_) {
            isWaiting_ = false;
            currentWaitTime_ = 0.0f;

            // 次のパトロールポイントへ
            if (patrolLoop_) {
                currentPatrolIndex_ = (currentPatrolIndex_ + 1) % patrolPoints_.size();
            } else {
                if (patrolReverse_) {
                    if (currentPatrolIndex_ == 0) {
                        patrolReverse_ = false;
                        currentPatrolIndex_ = 1;
                    } else {
                        --currentPatrolIndex_;
                    }
                } else {
                    if (currentPatrolIndex_ >= patrolPoints_.size() - 1) {
                        patrolReverse_ = true;
                        currentPatrolIndex_ = patrolPoints_.size() - 2;
                    } else {
                        ++currentPatrolIndex_;
                    }
                }
            }

            if (currentPatrolIndex_ < patrolPoints_.size()) {
                SetDestination(patrolPoints_[currentPatrolIndex_]);
            }
        }
        return;
    }

    // 直進モードで速度オーバーライド
    if (directMoveEnabled_ && crowdAgentIndex_ >= 0) {
        UpdateDirectMove();
    }

    auto& navMesh = Navigation::NavMeshManager::Get();

    if (crowdAgentIndex_ >= 0) {
        if (navMesh.HasAgentReachedTarget(crowdAgentIndex_, stoppingDistance_)) {
            isWaiting_ = true;
            currentWaitTime_ = 0.0f;
        }
    }
}

void NavAgentComponent::UpdateChase(float deltaTime) {
    if (!chaseTarget_) {
        Stop();
        return;
    }

    chaseUpdateTimer_ += deltaTime;

    if (chaseUpdateTimer_ >= chaseUpdateInterval_) {
        chaseUpdateTimer_ = 0.0f;

        auto targetPos = chaseTarget_->GetTransform().GetPosition();
        DirectX::XMFLOAT3 newDest = {targetPos.GetX(), targetPos.GetY(), targetPos.GetZ()};

        // 目的地が大きく変わった場合のみ更新
        float dx = newDest.x - destination_.x;
        float dz = newDest.z - destination_.z;
        float distSq = dx * dx + dz * dz;

        if (distSq > 1.0f) {
            auto& navMesh = Navigation::NavMeshManager::Get();
            if (crowdAgentIndex_ >= 0) {
                navMesh.SetAgentTarget(crowdAgentIndex_, newDest);
            }
            destination_ = newDest;
        }
    }

    // 直進モードで速度オーバーライド
    if (directMoveEnabled_ && crowdAgentIndex_ >= 0) {
        UpdateDirectMove();
    }
}

void NavAgentComponent::UpdateRotation(float deltaTime) {
    if (!gameObject_ || crowdAgentIndex_ < 0) {
        return;
    }

    auto& navMesh = Navigation::NavMeshManager::Get();
    auto& transform = gameObject_->GetTransform();

    constexpr float PI = 3.14159265f;

    // 次のコーナーへの方向を取得（速度よりも安定）
    DirectX::XMFLOAT3 nextCorner;
    float distToCorner = 0.0f;

    if (!navMesh.GetNextCorner(crowdAgentIndex_, nextCorner, distToCorner)) {
        return;
    }

    // 距離が短すぎる場合は回転しない
    if (distToCorner < 0.1f) {
        return;
    }

    // 現在位置からコーナーへの方向ベクトル
    auto agentPos = navMesh.GetAgentPosition(crowdAgentIndex_);
    float dx = nextCorner.x - agentPos.x;
    float dz = nextCorner.z - agentPos.z;

    // 目標Yaw角をコーナー方向から算出
    float targetYaw = std::atan2(dx, dz);

    // 初期化されていない場合は初期向きまたは現在の回転から初期化
    if (!yawInitialized_) {
        if (std::abs(initialYaw_) > 0.001f) {
            smoothedYaw_ = initialYaw_;
        } else {
            auto currentRot = transform.GetRotation();
            float sinY = 2.0f * (currentRot.GetW() * currentRot.GetY() - currentRot.GetZ() * currentRot.GetX());
            float cosY = 1.0f - 2.0f * (currentRot.GetX() * currentRot.GetX() + currentRot.GetY() * currentRot.GetY());
            smoothedYaw_ = std::atan2(sinY, cosY);
        }
        yawInitialized_ = true;
    }

    // 角度差分を-PI〜PIに正規化
    float angleDiff = targetYaw - smoothedYaw_;
    while (angleDiff > PI) angleDiff -= 2.0f * PI;
    while (angleDiff < -PI) angleDiff += 2.0f * PI;

    // 角速度制限（deg/sをrad/sに変換）
    float maxRotation = angularSpeed_ * 0.0174533f * deltaTime;

    // 角度差分を制限してスムーズに回転
    if (std::abs(angleDiff) > maxRotation) {
        float sign = (angleDiff > 0.0f) ? 1.0f : -1.0f;
        smoothedYaw_ += sign * maxRotation;
    } else {
        smoothedYaw_ = targetYaw;
    }

    // Yaw角度を-PI〜PIに正規化
    while (smoothedYaw_ > PI) smoothedYaw_ -= 2.0f * PI;
    while (smoothedYaw_ < -PI) smoothedYaw_ += 2.0f * PI;

    // クォータニオンに変換して適用
    float halfAngle = smoothedYaw_ * 0.5f;
    Quaternion newRot(0.0f, std::sin(halfAngle), 0.0f, std::cos(halfAngle));
    transform.SetRotation(newRot);
}

void NavAgentComponent::SyncTransformFromCrowd() {
    if (!gameObject_ || crowdAgentIndex_ < 0) {
        return;
    }
    
    auto& navMesh = Navigation::NavMeshManager::Get();
    auto agentPos = navMesh.GetAgentPosition(crowdAgentIndex_);
    
    auto& transform = gameObject_->GetTransform();
    transform.SetPosition(Vector3(agentPos.x, agentPos.y + baseOffset_, agentPos.z));
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
