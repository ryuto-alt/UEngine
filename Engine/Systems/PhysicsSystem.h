#pragma once

#include "ISystem.h"

namespace UnoEngine {

class PhysicsSystem : public ISystem {
public:
    PhysicsSystem() = default;
    ~PhysicsSystem() override = default;

    void OnSceneStart(Scene* scene) override;
    void OnUpdate(Scene* scene, float deltaTime) override;
    void OnSceneEnd(Scene* scene) override;

    // Lower priority = runs before CollisionSystem (priority 50)
    int GetPriority() const override { return 40; }

    static constexpr float kGravity = -30.0f;
};

} // namespace UnoEngine
