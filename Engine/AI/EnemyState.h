#pragma once

#include <memory>

namespace UnoEngine {

class EnemyDetectionComponent;

// State Pattern: 状態識別用enum（エディタ表示・シリアライズ用）
enum class EnemyStateType {
    Idle,
    Chasing,
    LostTarget
};

// 基底状態クラス
class EnemyState {
public:
    virtual ~EnemyState() = default;

    virtual auto Update(EnemyDetectionComponent& owner, float deltaTime)
        -> std::unique_ptr<EnemyState> = 0;

    virtual void Enter(EnemyDetectionComponent& owner) {}
    virtual void Exit(EnemyDetectionComponent& owner) {}

    virtual EnemyStateType GetType() const = 0;
};

// 待機・徘徊状態
class IdleState : public EnemyState {
public:
    auto Update(EnemyDetectionComponent& owner, float deltaTime)
        -> std::unique_ptr<EnemyState> override;

    void Enter(EnemyDetectionComponent& owner) override;
    EnemyStateType GetType() const override { return EnemyStateType::Idle; }
};

// 追跡状態
class ChasingState : public EnemyState {
public:
    auto Update(EnemyDetectionComponent& owner, float deltaTime)
        -> std::unique_ptr<EnemyState> override;

    void Enter(EnemyDetectionComponent& owner) override;
    EnemyStateType GetType() const override { return EnemyStateType::Chasing; }
};

// 見失った状態
class LostTargetState : public EnemyState {
public:
    auto Update(EnemyDetectionComponent& owner, float deltaTime)
        -> std::unique_ptr<EnemyState> override;

    void Enter(EnemyDetectionComponent& owner) override;
    EnemyStateType GetType() const override { return EnemyStateType::LostTarget; }

private:
    float timer_ = 0.0f;
};

} // namespace UnoEngine
