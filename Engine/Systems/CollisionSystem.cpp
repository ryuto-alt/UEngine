#include "pch.h"
#include "CollisionSystem.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"
#include "../Core/Logger.h"
#include <algorithm>

namespace UnoEngine {

void CollisionSystem::OnSceneStart(Scene* scene) {
    (void)scene;
    collisionComponents_.clear();
    currentCollisions_.clear();
    previousCollisions_.clear();

    // Initialize layer collision matrix (all layers collide by default)
    for (auto& row : layerCollisionMatrix_) {
        row = 0xFFFFFFFF;
    }
}

void CollisionSystem::OnUpdate(Scene* scene, float deltaTime) {
    (void)deltaTime;

    if (!IsEnabled()) return;

    GatherCollisionComponents(scene);
    CheckCollisions();
    UpdateCollisionStates();
}

void CollisionSystem::OnSceneEnd(Scene* scene) {
    (void)scene;
    collisionComponents_.clear();
    currentCollisions_.clear();
    previousCollisions_.clear();
}

void CollisionSystem::GatherCollisionComponents(Scene* scene) {
    collisionComponents_.clear();

    if (!scene) return;

    auto& gameObjects = scene->GetGameObjects();
    for (auto& go : gameObjects) {
        if (!go || !go->IsActive()) continue;

        auto* collision = go->GetComponent<CollisionComponent>();
        if (collision && collision->IsEnabled()) {
            collisionComponents_.push_back(collision);
        }
    }
}

void CollisionSystem::CheckCollisions() {
    previousCollisions_ = std::move(currentCollisions_);
    currentCollisions_.clear();

    // Reset collision states
    for (auto* comp : collisionComponents_) {
        comp->SetColliding(false);
    }

    // O(n^2) broad phase - for small object counts this is acceptable
    // For larger scenes, consider spatial partitioning (octree, grid, etc.)
    for (size_t i = 0; i < collisionComponents_.size(); ++i) {
        for (size_t j = i + 1; j < collisionComponents_.size(); ++j) {
            auto* a = collisionComponents_[i];
            auto* b = collisionComponents_[j];

            if (!ShouldCheckCollision(a, b)) continue;

            AABB aabbA = a->GetWorldAABB();
            AABB aabbB = b->GetWorldAABB();

            if (CollisionComponent::CheckAABBCollision(aabbA, aabbB)) {
                a->SetColliding(true);
                b->SetColliding(true);

                CollisionPair pair;
                pair.a = a;
                pair.b = b;
                currentCollisions_.push_back(pair);
            }
        }
    }
}

void CollisionSystem::UpdateCollisionStates() {
    // Find new collisions (enter)
    for (const auto& current : currentCollisions_) {
        bool wasColliding = false;
        for (const auto& prev : previousCollisions_) {
            if ((prev.a == current.a && prev.b == current.b) ||
                (prev.a == current.b && prev.b == current.a)) {
                wasColliding = true;
                break;
            }
        }

        if (!wasColliding) {
            // New collision - call OnCollisionEnter
            if (onCollisionEnter_) {
                onCollisionEnter_(current.a, current.b);
            }
        } else {
            // Continuing collision - call OnCollisionStay
            if (onCollisionStay_) {
                onCollisionStay_(current.a, current.b);
            }
        }
    }

    // Find ended collisions (exit)
    for (const auto& prev : previousCollisions_) {
        bool stillColliding = false;
        for (const auto& current : currentCollisions_) {
            if ((prev.a == current.a && prev.b == current.b) ||
                (prev.a == current.b && prev.b == current.a)) {
                stillColliding = true;
                break;
            }
        }

        if (!stillColliding) {
            // Collision ended - call OnCollisionExit
            if (onCollisionExit_) {
                onCollisionExit_(prev.a, prev.b);
            }
        }
    }
}

bool CollisionSystem::ShouldCheckCollision(CollisionComponent* a, CollisionComponent* b) const {
    if (!a || !b) return false;
    if (!a->IsEnabled() || !b->IsEnabled()) return false;

    // Check layer mask
    uint32_t layerA = a->GetCollisionLayer();
    uint32_t layerB = b->GetCollisionLayer();

    // Check if a's mask includes b's layer and vice versa
    if ((a->GetCollisionMask() & (1u << layerB)) == 0) return false;
    if ((b->GetCollisionMask() & (1u << layerA)) == 0) return false;

    // Check layer collision matrix
    if (!CanLayersCollide(layerA, layerB)) return false;

    return true;
}

void CollisionSystem::SetLayerCollision(uint32_t layerA, uint32_t layerB, bool canCollide) {
    if (layerA >= 32 || layerB >= 32) return;

    if (canCollide) {
        layerCollisionMatrix_[layerA] |= (1u << layerB);
        layerCollisionMatrix_[layerB] |= (1u << layerA);
    } else {
        layerCollisionMatrix_[layerA] &= ~(1u << layerB);
        layerCollisionMatrix_[layerB] &= ~(1u << layerA);
    }
}

bool CollisionSystem::CanLayersCollide(uint32_t layerA, uint32_t layerB) const {
    if (layerA >= 32 || layerB >= 32) return false;
    return (layerCollisionMatrix_[layerA] & (1u << layerB)) != 0;
}

} // namespace UnoEngine
