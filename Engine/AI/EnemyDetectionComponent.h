#pragma once

#include "../Core/Component.h"
#include <DirectXMath.h>

namespace UnoEngine {

class GameObject;
class NavAgentComponent;
class Scene;

// プレイヤーを検知して追跡するAIコンポーネント
class EnemyDetectionComponent : public Component {
public:
    enum class State {
        Idle,       // 待機中（徘徊など）
        Chasing,    // 追跡中
        LostTarget  // 見失った（停止中）
    };

    EnemyDetectionComponent() = default;
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

    // 徘徊設定
    float GetWanderRadius() const { return wanderRadius_; }
    void SetWanderRadius(float radius) { wanderRadius_ = radius; }

    // ターゲット名
    const std::string& GetTargetName() const { return targetName_; }
    void SetTargetName(const std::string& name) { targetName_ = name; }

    // 状態取得
    State GetState() const { return state_; }

    // Scene参照（ターゲット検索用）
    void SetScene(Scene* scene) { scene_ = scene; }

private:
    GameObject* FindTarget();
    bool IsTargetInSight(GameObject* target);
    float GetDistanceToTarget(GameObject* target);
    bool IsTargetInFOV(GameObject* target);
    void StartChasing(GameObject* target);
    void StopChasing();
    void StartWandering();

    NavAgentComponent* navAgent_ = nullptr;
    GameObject* currentTarget_ = nullptr;
    Scene* scene_ = nullptr;
    State state_ = State::Idle;

    // 検知パラメータ
    float detectionRange_ = 8.0f;    // 検知距離
    float fieldOfView_ = 90.0f;      // 視野角（度）
    float loseRange_ = 15.0f;        // 見失う距離
    float lostWaitTime_ = 3.0f;      // 見失い後の待機時間

    // 徘徊パラメータ
    float wanderRadius_ = 10.0f;

    // タイマー
    float lostTimer_ = 0.0f;

    // ターゲット識別
    std::string targetName_ = "player";
};

} // namespace UnoEngine
