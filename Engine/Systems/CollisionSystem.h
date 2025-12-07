#pragma once

#include "ISystem.h"
#include "../Core/CollisionComponent.h"
#include <vector>
#include <functional>

namespace UnoEngine {

class GameObject;

struct CollisionPair {
    CollisionComponent* a = nullptr;
    CollisionComponent* b = nullptr;
};

using CollisionCallback = std::function<void(CollisionComponent*, CollisionComponent*)>;

class CollisionSystem : public ISystem {
public:
    CollisionSystem() = default;
    ~CollisionSystem() override = default;

    void OnSceneStart(Scene* scene) override;
    void OnUpdate(Scene* scene, float deltaTime) override;
    void OnSceneEnd(Scene* scene) override;

    int GetPriority() const override { return 50; }

    // Collision event callbacks
    void SetOnCollisionEnter(CollisionCallback callback) { onCollisionEnter_ = std::move(callback); }
    void SetOnCollisionStay(CollisionCallback callback) { onCollisionStay_ = std::move(callback); }
    void SetOnCollisionExit(CollisionCallback callback) { onCollisionExit_ = std::move(callback); }

    // Debug visualization
    void SetDebugDraw(bool enabled) { debugDraw_ = enabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const { return debugDraw_; }

    // Get current collision pairs (for debug/visualization)
    [[nodiscard]] const std::vector<CollisionPair>& GetCurrentCollisions() const { return currentCollisions_; }

    // Layer collision matrix
    void SetLayerCollision(uint32_t layerA, uint32_t layerB, bool canCollide);
    [[nodiscard]] bool CanLayersCollide(uint32_t layerA, uint32_t layerB) const;

private:
    void GatherCollisionComponents(Scene* scene);
    void CheckCollisions();
    void UpdateCollisionStates();
    bool ShouldCheckCollision(CollisionComponent* a, CollisionComponent* b) const;

    std::vector<CollisionComponent*> collisionComponents_;
    std::vector<CollisionPair> currentCollisions_;
    std::vector<CollisionPair> previousCollisions_;

    CollisionCallback onCollisionEnter_;
    CollisionCallback onCollisionStay_;
    CollisionCallback onCollisionExit_;

    bool debugDraw_ = false;

    // Simple layer collision matrix (32 layers max)
    uint32_t layerCollisionMatrix_[32] = { 0xFFFFFFFF };
};

} // namespace UnoEngine
