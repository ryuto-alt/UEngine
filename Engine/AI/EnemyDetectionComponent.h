#pragma once

#include "../Core/Component.h"
#include "EnemyState.h"
#include <DirectXMath.h>
#include <memory>
#include <numbers>

namespace UnoEngine {

class GameObject;
class NavAgentComponent;
class Scene;

// プレイヤーを検知して追跡するAIコンポーネント
class EnemyDetectionComponent : public Component {
public:
    // 数学定数
    static constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
    static constexpr float kNormalizationEpsilon = 0.001f;

    EnemyDetectionComponent();
    ~EnemyDetectionComponent() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate(float deltaTime) override;

    // 検知設定
    float GetDetectionRange() const { return detectionRange_; }
    void SetDetectionRange(float range) { detectionRange_ = range; }

    float GetFieldOfView() const { return fieldOfView_; }
    void SetFieldOfView(float fov) { fieldOfView_ = fov; }

    float GetLoseRange() const { return loseRange_; }
    void SetLoseRange(float range) { loseRange_ = range; }

    float GetLostWaitTime() const { return lostWaitTime_; }
    void SetLostWaitTime(float time) { lostWaitTime_ = time; }

    float GetChaseStoppingDistance() const { return chaseStoppingDistance_; }
    void SetChaseStoppingDistance(float dist) { chaseStoppingDistance_ = dist; }

    // 徘徊設定
    float GetWanderRadius() const { return wanderRadius_; }
    void SetWanderRadius(float radius) { wanderRadius_ = radius; }

    // ターゲット名
    const std::string& GetTargetName() const { return targetName_; }
    void SetTargetName(const std::string& name) { targetName_ = name; }

    // 状態取得（エディタ表示・シリアライズ用）
    EnemyStateType GetStateType() const;

    // Scene参照（ターゲット検索用）
    void SetScene(Scene* scene) { scene_ = scene; }

    // State Patternから呼ばれるヘルパー（publicにしてStateクラスからアクセス可能に）
    GameObject* FindTarget();
    bool IsTargetInFOV(GameObject* target);
    float GetDistanceToTarget(GameObject* target);
    void StartChasing(GameObject* target);
    void StopChasing();
    void StartWandering();

    NavAgentComponent* GetNavAgent() const { return navAgent_; }
    GameObject* GetCurrentTarget() const { return currentTarget_; }
    void SetCurrentTarget(GameObject* target) { currentTarget_ = target; }

private:
    std::unique_ptr<EnemyState> currentState_;
    NavAgentComponent* navAgent_ = nullptr;
    GameObject* currentTarget_ = nullptr;
    Scene* scene_ = nullptr;

    // 検知パラメータ
    float detectionRange_ = 8.0f;
    float fieldOfView_ = 90.0f;
    float loseRange_ = 15.0f;
    float lostWaitTime_ = 3.0f;
    float chaseStoppingDistance_ = 0.2f;

    // 徘徊パラメータ
    float wanderRadius_ = 10.0f;

    // ターゲット識別
    std::string targetName_ = "player";
};

} // namespace UnoEngine
