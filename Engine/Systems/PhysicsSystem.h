#pragma once

#include "ISystem.h"

namespace UnoEngine {

class PhysicsSystem : public ISystem {
public:
    PhysicsSystem();
    ~PhysicsSystem() override = default;

    void OnSceneStart(Scene* scene) override;
    void OnUpdate(Scene* scene, float deltaTime) override;
    void OnSceneEnd(Scene* scene) override;

    int GetPriority() const override { return 40; }

    float GetGravity() const { return gravity_; }
    void SetGravity(float g) { gravity_ = g; }

private:
    // assets/config/physics.json から読み込み。デフォルト -30.0
    float gravity_ = -30.0f;

    void LoadConfig();
};

} // namespace UnoEngine
