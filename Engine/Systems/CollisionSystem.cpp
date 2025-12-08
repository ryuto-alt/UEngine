#include "pch.h"
#include "pch.h"
#include "CollisionSystem.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"
#include "../Core/Logger.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

#ifdef _DEBUG
#include "../../Game/UI/EditorUI.h"
#endif

namespace UnoEngine {

void CollisionSystem::OnSceneStart(Scene* scene) {
    (void)scene;
    collisionComponents_.clear();
    currentCollisions_.clear();
    previousCollisions_.clear();

    // Initialize layer collision matrix (all layers collide by default)
    for (size_t i = 0; i < 32; ++i) {
        layerCollisionMatrix_[i] = 0xFFFFFFFF;
    }

    Logger::Debug("[Collision] System started - all 32 layers initialized to collide");
}

void CollisionSystem::OnUpdate(Scene* scene, float deltaTime) {
    (void)deltaTime;

    if (!IsEnabled()) return;
    if (!scene) return;

#ifdef _DEBUG
    auto* editorUI = scene->GetEditorUI();
    if (editorUI && !editorUI->IsPlaying()) {
        return;
    }
#endif

    GatherCollisionComponents(scene);
    CheckCollisions();
    ResolveCollisions();
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

    // Debug: show collision components every 2 seconds
    static int frameCount = 0;
    if (++frameCount % 120 == 0) {
        Logger::Debug("[Collision] Active components: {}", collisionComponents_.size());
        for (auto* comp : collisionComponents_) {
            auto* go = comp->GetGameObject();
            auto worldAABBs = comp->GetWorldAABBs();
            Logger::Debug("  - {} layer={} trigger={} static={} aabbCount={}",
                go ? go->GetName() : "null",
                comp->GetCollisionLayer(),
                comp->IsTrigger(),
                comp->IsStatic(),
                worldAABBs.size());
            for (size_t i = 0; i < worldAABBs.size(); ++i) {
                const auto& aabb = worldAABBs[i];
                Logger::Debug("    AABB[{}]: ({:.2f},{:.2f},{:.2f}) - ({:.2f},{:.2f},{:.2f})",
                    i,
                    aabb.min.GetX(), aabb.min.GetY(), aabb.min.GetZ(),
                    aabb.max.GetX(), aabb.max.GetY(), aabb.max.GetZ());
            }
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

    static int debugFrame = 0;
    bool shouldLog = (++debugFrame % 120 == 0);

    for (size_t i = 0; i < collisionComponents_.size(); ++i) {
        for (size_t j = i + 1; j < collisionComponents_.size(); ++j) {
            auto* a = collisionComponents_[i];
            auto* b = collisionComponents_[j];

            if (!ShouldCheckCollision(a, b)) {
                if (shouldLog) {
                    Logger::Debug("[Collision] ShouldCheckCollision returned false for pair {}-{}", i, j);
                }
                continue;
            }

            bool collisionFound = false;
            Vector3 totalPenetration(0.0f, 0.0f, 0.0f);

            // Check with multiple AABBs if available
            auto aabbsA = a->GetWorldAABBs();
            auto aabbsB = b->GetWorldAABBs();
            
            if (shouldLog) {
                Logger::Debug("[Collision] Checking {} AABBs vs {} AABBs", aabbsA.size(), aabbsB.size());
                if (!aabbsA.empty()) {
                    auto& box = aabbsA[0];
                    Logger::Debug("[Collision] A[0]: min({:.2f},{:.2f},{:.2f}) max({:.2f},{:.2f},{:.2f})",
                        box.min.GetX(), box.min.GetY(), box.min.GetZ(),
                        box.max.GetX(), box.max.GetY(), box.max.GetZ());
                }
                if (!aabbsB.empty()) {
                    auto& box = aabbsB[0];
                    Logger::Debug("[Collision] B[0]: min({:.2f},{:.2f},{:.2f}) max({:.2f},{:.2f},{:.2f})",
                        box.min.GetX(), box.min.GetY(), box.min.GetZ(),
                        box.max.GetX(), box.max.GetY(), box.max.GetZ());
                }
            }

            // 各軸の押し戻し方向を追跡（正・負・両方）
            bool hasPositiveX = false, hasNegativeX = false;
            bool hasPositiveY = false, hasNegativeY = false;
            bool hasPositiveZ = false, hasNegativeZ = false;
            float bestPenX = 0.0f, bestPenY = 0.0f, bestPenZ = 0.0f;
            float minAbsX = FLT_MAX, minAbsY = FLT_MAX, minAbsZ = FLT_MAX;
            
            bool hasStatic = a->IsStatic() || b->IsStatic();
            
            for (const auto& aabbA : aabbsA) {
                for (const auto& aabbB : aabbsB) {
                    if (CollisionComponent::CheckAABBCollision(aabbA, aabbB)) {
                        collisionFound = true;
                        Vector3 pen = CollisionComponent::GetPenetrationVector(aabbA, aabbB);
                        
                        // 各軸のペネトレーションを追跡
                        if (std::abs(pen.GetX()) > 0.0001f) {
                            if (pen.GetX() > 0) hasPositiveX = true;
                            else hasNegativeX = true;
                            if (std::abs(pen.GetX()) < minAbsX) {
                                minAbsX = std::abs(pen.GetX());
                                bestPenX = pen.GetX();
                            }
                        }
                        if (std::abs(pen.GetY()) > 0.0001f) {
                            if (pen.GetY() > 0) hasPositiveY = true;
                            else hasNegativeY = true;
                            if (std::abs(pen.GetY()) < minAbsY) {
                                minAbsY = std::abs(pen.GetY());
                                bestPenY = pen.GetY();
                            }
                        }
                        if (std::abs(pen.GetZ()) > 0.0001f) {
                            if (pen.GetZ() > 0) hasPositiveZ = true;
                            else hasNegativeZ = true;
                            if (std::abs(pen.GetZ()) < minAbsZ) {
                                minAbsZ = std::abs(pen.GetZ());
                                bestPenZ = pen.GetZ();
                            }
                        }
                    }
                }
            }
            
            // 反対方向の衝突がある軸は無視（壁に挟まれている）
            if (hasPositiveX && hasNegativeX) { bestPenX = 0.0f; minAbsX = FLT_MAX; }
            if (hasPositiveY && hasNegativeY) { bestPenY = 0.0f; minAbsY = FLT_MAX; }
            if (hasPositiveZ && hasNegativeZ) { bestPenZ = 0.0f; minAbsZ = FLT_MAX; }
            
            // 壁との衝突ではY軸を無視
            if (hasStatic && (minAbsX < FLT_MAX || minAbsZ < FLT_MAX)) {
                minAbsY = FLT_MAX;
                bestPenY = 0.0f;
            }
            
            // 最小の脱出軸を選択
            if (minAbsX <= minAbsY && minAbsX <= minAbsZ && minAbsX < FLT_MAX) {
                totalPenetration = Vector3(bestPenX, 0.0f, 0.0f);
            } else if (minAbsY <= minAbsX && minAbsY <= minAbsZ && minAbsY < FLT_MAX) {
                totalPenetration = Vector3(0.0f, bestPenY, 0.0f);
            } else if (minAbsZ < FLT_MAX) {
                totalPenetration = Vector3(0.0f, 0.0f, bestPenZ);
            }

            if (collisionFound) {
                a->SetColliding(true);
                b->SetColliding(true);

                CollisionPair pair;
                pair.a = a;
                pair.b = b;
                pair.penetration = totalPenetration;
                currentCollisions_.push_back(pair);

                auto* goA = a->GetGameObject();
                auto* goB = b->GetGameObject();
                Logger::Debug("[Collision] HIT: {} <-> {} pen({:.3f},{:.3f},{:.3f})",
                    goA ? goA->GetName() : "null",
                    goB ? goB->GetName() : "null",
                    totalPenetration.GetX(), totalPenetration.GetY(), totalPenetration.GetZ());
            }
        }
    }
}

void CollisionSystem::ResolveCollisions() {
    for (const auto& pair : currentCollisions_) {
        // Skip if either is a trigger
        if (pair.a->IsTrigger() || pair.b->IsTrigger()) {
            continue;
        }

        auto* goA = pair.a->GetGameObject();
        auto* goB = pair.b->GetGameObject();
        if (!goA || !goB) continue;

        bool aStatic = pair.a->IsStatic();
        bool bStatic = pair.b->IsStatic();

        // If both static, no resolution needed
        if (aStatic && bStatic) continue;

        // ペネトレーションは既にCheckCollisionsで最小脱出軸が選択済み
        Vector3 resolution = pair.penetration;
        
        // ジッタリング防止：押し戻し方向に小さなマージンを追加
        constexpr float margin = 0.01f;
        float marginX = (resolution.GetX() > 0) ? margin : (resolution.GetX() < 0) ? -margin : 0.0f;
        float marginY = (resolution.GetY() > 0) ? margin : (resolution.GetY() < 0) ? -margin : 0.0f;
        float marginZ = (resolution.GetZ() > 0) ? margin : (resolution.GetZ() < 0) ? -margin : 0.0f;
        resolution = Vector3(
            resolution.GetX() + marginX,
            resolution.GetY() + marginY,
            resolution.GetZ() + marginZ
        );

        if (aStatic) {
            // Only move B
            auto& transformB = goB->GetTransform();
            Vector3 posB = transformB.GetPosition();
            transformB.SetPosition(Vector3(
                posB.GetX() - resolution.GetX(),
                posB.GetY() - resolution.GetY(),
                posB.GetZ() - resolution.GetZ()
            ));
        } else if (bStatic) {
            // Only move A
            auto& transformA = goA->GetTransform();
            Vector3 posA = transformA.GetPosition();
            transformA.SetPosition(Vector3(
                posA.GetX() + resolution.GetX(),
                posA.GetY() + resolution.GetY(),
                posA.GetZ() + resolution.GetZ()
            ));
        } else {
            // Both dynamic - split the resolution
            auto& transformA = goA->GetTransform();
            auto& transformB = goB->GetTransform();
            Vector3 posA = transformA.GetPosition();
            Vector3 posB = transformB.GetPosition();

            transformA.SetPosition(Vector3(
                posA.GetX() + resolution.GetX() * 0.5f,
                posA.GetY() + resolution.GetY() * 0.5f,
                posA.GetZ() + resolution.GetZ() * 0.5f
            ));
            transformB.SetPosition(Vector3(
                posB.GetX() - resolution.GetX() * 0.5f,
                posB.GetY() - resolution.GetY() * 0.5f,
                posB.GetZ() - resolution.GetZ() * 0.5f
            ));
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

    uint32_t layerA = a->GetCollisionLayer();
    uint32_t layerB = b->GetCollisionLayer();
    uint32_t maskA = a->GetCollisionMask();
    uint32_t maskB = b->GetCollisionMask();

    // Check if a's mask includes b's layer and vice versa
    bool maskCheck1 = (maskA & (1u << layerB)) != 0;
    bool maskCheck2 = (maskB & (1u << layerA)) != 0;
    bool matrixCheck = CanLayersCollide(layerA, layerB);

    if (!maskCheck1 || !maskCheck2 || !matrixCheck) {
        static int logCount = 0;
        if (logCount++ < 5) {
            Logger::Debug("[Collision] ShouldCheck FAILED: layerA={} layerB={} maskA={:08X} maskB={:08X} "
                         "check1={} check2={} matrix={}",
                         layerA, layerB, maskA, maskB, maskCheck1, maskCheck2, matrixCheck);
        }
        return false;
    }

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
