#include "pch.h"
#include "CollisionComponent.h"
#include "GameObject.h"
#include "Transform.h"
#include "../Graphics/MeshRenderer.h"
#include "../Resource/StaticModelImporter.h"
#include "../Rendering/SkinnedMeshRenderer.h"
#include "Logger.h"
#include <cmath>
#include <algorithm>

namespace UnoEngine {

void CollisionComponent::Awake() {
}

void CollisionComponent::Start() {
    if (useAutoSize_) {
        RecalculateFromMesh();
    }
}

void CollisionComponent::OnUpdate(float deltaTime) {
    (void)deltaTime;
}

static AABB TransformLocalAABB(const AABB& local, const Vector3& worldPos, const Vector3& worldScale) {
    Vector3 scaledMin(
        local.min.GetX() * worldScale.GetX(),
        local.min.GetY() * worldScale.GetY(),
        local.min.GetZ() * worldScale.GetZ()
    );
    Vector3 scaledMax(
        local.max.GetX() * worldScale.GetX(),
        local.max.GetY() * worldScale.GetY(),
        local.max.GetZ() * worldScale.GetZ()
    );

    AABB worldAABB;
    worldAABB.min = Vector3(
        worldPos.GetX() + scaledMin.GetX(),
        worldPos.GetY() + scaledMin.GetY(),
        worldPos.GetZ() + scaledMin.GetZ()
    );
    worldAABB.max = Vector3(
        worldPos.GetX() + scaledMax.GetX(),
        worldPos.GetY() + scaledMax.GetY(),
        worldPos.GetZ() + scaledMax.GetZ()
    );

    // Ensure min < max after scaling
    if (worldAABB.min.GetX() > worldAABB.max.GetX()) {
        std::swap(worldAABB.min, worldAABB.max);
        worldAABB.min = Vector3(worldAABB.max.GetX(), worldAABB.min.GetY(), worldAABB.min.GetZ());
        worldAABB.max = Vector3(worldAABB.min.GetX(), worldAABB.max.GetY(), worldAABB.max.GetZ());
    }

    float minX = std::min(worldAABB.min.GetX(), worldAABB.max.GetX());
    float maxX = std::max(worldAABB.min.GetX(), worldAABB.max.GetX());
    float minY = std::min(worldAABB.min.GetY(), worldAABB.max.GetY());
    float maxY = std::max(worldAABB.min.GetY(), worldAABB.max.GetY());
    float minZ = std::min(worldAABB.min.GetZ(), worldAABB.max.GetZ());
    float maxZ = std::max(worldAABB.min.GetZ(), worldAABB.max.GetZ());

    worldAABB.min = Vector3(minX, minY, minZ);
    worldAABB.max = Vector3(maxX, maxY, maxZ);

    return worldAABB;
}

AABB CollisionComponent::GetWorldAABB() const {
    if (!GetGameObject()) {
        return localAABB_;
    }

    auto& transform = GetGameObject()->GetTransform();
    Vector3 worldPos = transform.GetPosition();
    Vector3 worldScale = transform.GetScale();

    return TransformLocalAABB(localAABB_, worldPos, worldScale);
}

std::vector<AABB> CollisionComponent::GetWorldAABBs() const {
    std::vector<AABB> result;
    
    if (!GetGameObject()) {
        if (!localAABBs_.empty()) {
            return localAABBs_;
        }
        result.push_back(localAABB_);
        return result;
    }

    auto& transform = GetGameObject()->GetTransform();
    Vector3 worldPos = transform.GetPosition();
    Vector3 worldScale = transform.GetScale();

    if (!localAABBs_.empty()) {
        result.reserve(localAABBs_.size());
        for (const auto& local : localAABBs_) {
            result.push_back(TransformLocalAABB(local, worldPos, worldScale));
        }
    } else {
        result.push_back(TransformLocalAABB(localAABB_, worldPos, worldScale));
    }

    return result;
}

void CollisionComponent::SetLocalAABB(const Vector3& min, const Vector3& max) {
    localAABB_.min = min;
    localAABB_.max = max;
    localAABBs_.clear();
    useAutoSize_ = false;
}

void CollisionComponent::RecalculateFromMesh() {
    localAABBs_.clear();
    
    if (!GetGameObject()) {
        Logger::Debug("[Collision] RecalculateFromMesh: No GameObject");
        return;
    }

    auto* go = GetGameObject();
    Logger::Info("[Collision] RecalculateFromMesh for: {}", go->GetName());

    // Try MeshRenderer (static models with multiple meshes)
    auto* meshRenderer = go->GetComponent<MeshRenderer>();
    if (meshRenderer) {
        Logger::Info("[Collision] Found MeshRenderer, HasModel={}", meshRenderer->HasModel());
        if (meshRenderer->HasModel()) {
            auto* modelData = meshRenderer->GetModel();
            if (modelData) {
                // Combined bounding box
                localAABB_.min = modelData->boundingBox.min;
                localAABB_.max = modelData->boundingBox.max;

                Logger::Info("[Collision] {} has {} meshes", go->GetName(), modelData->meshes.size());

                // Per-mesh AABBs for complex models - always create per-mesh AABBs
                localAABBs_.reserve(modelData->meshes.size());
                for (size_t i = 0; i < modelData->meshes.size(); ++i) {
                    const auto& mesh = modelData->meshes[i];
                    AABB meshAABB;
                    meshAABB.min = mesh.GetBoundsMin();
                    meshAABB.max = mesh.GetBoundsMax();
                    localAABBs_.push_back(meshAABB);
                }
                Logger::Info("[Collision] Created {} AABBs for {}", localAABBs_.size(), go->GetName());
                return;
            }
        }
    } else {
        Logger::Info("[Collision] No MeshRenderer found on {}", go->GetName());
    }

    // Try SkinnedMeshRenderer
    auto* skinnedRenderer = GetGameObject()->GetComponent<SkinnedMeshRenderer>();
    if (skinnedRenderer && skinnedRenderer->HasModel()) {
        auto* modelData = skinnedRenderer->GetModelData();
        if (modelData) {
            localAABB_.min = modelData->boundingBox.min;
            localAABB_.max = modelData->boundingBox.max;
            Logger::Debug("[Collision] Auto-sized AABB from SkinnedMeshRenderer");
            return;
        }
    }

    // Default unit cube if no mesh found
    localAABB_.min = Vector3(-0.5f, -0.5f, -0.5f);
    localAABB_.max = Vector3(0.5f, 0.5f, 0.5f);
}

bool CollisionComponent::CheckAABBCollision(const AABB& a, const AABB& b) {
    // 厳密に分離している場合のみfalse（接触はOK、重なりのみ衝突）
    constexpr float epsilon = 0.0001f;
    if (a.max.GetX() <= b.min.GetX() + epsilon || a.min.GetX() >= b.max.GetX() - epsilon) return false;
    if (a.max.GetY() <= b.min.GetY() + epsilon || a.min.GetY() >= b.max.GetY() - epsilon) return false;
    if (a.max.GetZ() <= b.min.GetZ() + epsilon || a.min.GetZ() >= b.max.GetZ() - epsilon) return false;
    return true;
}

Vector3 CollisionComponent::GetPenetrationVector(const AABB& a, const AABB& b) {
    // Calculate overlap on each axis - 全軸のめり込み量を返す
    float overlapX1 = b.max.GetX() - a.min.GetX();
    float overlapX2 = a.max.GetX() - b.min.GetX();
    float overlapY1 = b.max.GetY() - a.min.GetY();
    float overlapY2 = a.max.GetY() - b.min.GetY();
    float overlapZ1 = b.max.GetZ() - a.min.GetZ();
    float overlapZ2 = a.max.GetZ() - b.min.GetZ();

    float overlapX = (overlapX1 < overlapX2) ? overlapX1 : -overlapX2;
    float overlapY = (overlapY1 < overlapY2) ? overlapY1 : -overlapY2;
    float overlapZ = (overlapZ1 < overlapZ2) ? overlapZ1 : -overlapZ2;

    // 全軸のペネトレーションを返す（最小軸の選択はResolveCollisionsで行う）
    return Vector3(overlapX, overlapY, overlapZ);
}

} // namespace UnoEngine
