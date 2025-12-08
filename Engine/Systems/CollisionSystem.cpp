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
    
    // 複数回イテレーションで衝突解決（トンネル効果と押し戻し後の再衝突を防ぐ）
    constexpr int maxIterations = 4;
    for (int iter = 0; iter < maxIterations; ++iter) {
        CheckCollisions();
        if (currentCollisions_.empty()) break;
        ResolveCollisions();
    }
    
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
                continue;
            }

            bool collisionFound = false;
            
            auto aabbsA = a->GetWorldAABBs();
            auto aabbsB = b->GetWorldAABBs();
            
            if (shouldLog) {
                Logger::Debug("[Collision] Checking {} AABBs vs {} AABBs", aabbsA.size(), aabbsB.size());
            }

            // 各AABBペアごとに最小分離軸を計算し、軸ごとに蓄積
            float maxPosX = 0.0f, maxNegX = 0.0f;
            float maxPosY = 0.0f, maxNegY = 0.0f;
            float maxPosZ = 0.0f, maxNegZ = 0.0f;
            
            bool hasStatic = a->IsStatic() || b->IsStatic();
            
            for (const auto& aabbA : aabbsA) {
                for (const auto& aabbB : aabbsB) {
                    if (!CollisionComponent::CheckAABBCollision(aabbA, aabbB)) {
                        continue;
                    }
                    
                    collisionFound = true;
                    
                    // このAABBペアの最小分離軸を計算
                    Vector3 pen = CollisionComponent::GetPenetrationVector(aabbA, aabbB);
                    float absX = std::abs(pen.GetX());
                    float absY = std::abs(pen.GetY());
                    float absZ = std::abs(pen.GetZ());
                    
                    // 壁との衝突ではY軸を無視
                    if (hasStatic && (absX > 0.0001f || absZ > 0.0001f)) {
                        absY = FLT_MAX;
                    }
                    
                    // このペアの最小分離軸を選択
                    float mtv = 0.0f;
                    int axis = -1;  // 0=X, 1=Y, 2=Z
                    
                    if (absX <= absY && absX <= absZ && absX > 0.0001f) {
                        mtv = pen.GetX();
                        axis = 0;
                    } else if (absY <= absX && absY <= absZ && absY > 0.0001f && absY < FLT_MAX) {
                        mtv = pen.GetY();
                        axis = 1;
                    } else if (absZ > 0.0001f) {
                        mtv = pen.GetZ();
                        axis = 2;
                    }
                    
                    // 軸ごとに押し戻しを蓄積（同方向は最大値、反対方向は別々に追跡）
                    if (axis == 0) {
                        if (mtv > 0.0001f) maxPosX = std::max(maxPosX, mtv);
                        else if (mtv < -0.0001f) maxNegX = std::min(maxNegX, mtv);
                    } else if (axis == 1) {
                        if (mtv > 0.0001f) maxPosY = std::max(maxPosY, mtv);
                        else if (mtv < -0.0001f) maxNegY = std::min(maxNegY, mtv);
                    } else if (axis == 2) {
                        if (mtv > 0.0001f) maxPosZ = std::max(maxPosZ, mtv);
                        else if (mtv < -0.0001f) maxNegZ = std::min(maxNegZ, mtv);
                    }
                }
            }
            
            if (!collisionFound) continue;
            
            // 各軸の最終的な押し戻し量を決定
            float pushX = 0.0f, pushY = 0.0f, pushZ = 0.0f;
            
            // X軸: 両方向なら挟まれている、片方のみなら採用
            if (maxPosX > 0.0001f && maxNegX < -0.0001f) {
                pushX = 0.0f;  // 挟まれている
            } else if (maxPosX > 0.0001f) {
                pushX = maxPosX;
            } else if (maxNegX < -0.0001f) {
                pushX = maxNegX;
            }
            
            // Y軸
            if (maxPosY > 0.0001f && maxNegY < -0.0001f) {
                pushY = 0.0f;
            } else if (maxPosY > 0.0001f) {
                pushY = maxPosY;
            } else if (maxNegY < -0.0001f) {
                pushY = maxNegY;
            }
            
            // Z軸
            if (maxPosZ > 0.0001f && maxNegZ < -0.0001f) {
                pushZ = 0.0f;
            } else if (maxPosZ > 0.0001f) {
                pushZ = maxPosZ;
            } else if (maxNegZ < -0.0001f) {
                pushZ = maxNegZ;
            }
            
            Vector3 totalPenetration(pushX, pushY, pushZ);

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

        Vector3 resolution = pair.penetration;
        
        // 押し戻し後の再衝突を防ぐマージン（CheckAABBCollisionのepsilonより大きく）
        constexpr float margin = 0.002f;
        float resX = resolution.GetX();
        float resY = resolution.GetY();
        float resZ = resolution.GetZ();
        
        if (std::abs(resX) > 0.0001f) {
            resX += (resX > 0) ? margin : -margin;
        }
        if (std::abs(resY) > 0.0001f) {
            resY += (resY > 0) ? margin : -margin;
        }
        if (std::abs(resZ) > 0.0001f) {
            resZ += (resZ > 0) ? margin : -margin;
        }
        
        resolution = Vector3(resX, resY, resZ);

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
