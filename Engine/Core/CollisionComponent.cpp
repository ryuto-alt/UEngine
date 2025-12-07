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
    if (useAutoSize_) {
        RecalculateFromMesh();
    }
}

void CollisionComponent::Start() {
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
        return;
    }

    // Try MeshRenderer (static models with multiple meshes)
    auto* meshRenderer = GetGameObject()->GetComponent<MeshRenderer>();
    if (meshRenderer && meshRenderer->HasModel()) {
        auto* modelData = meshRenderer->GetModel();
        if (modelData) {
            // Combined bounding box
            localAABB_.min = modelData->boundingBox.min;
            localAABB_.max = modelData->boundingBox.max;

            // Per-mesh AABBs for complex models
            if (modelData->meshes.size() > 1) {
                localAABBs_.reserve(modelData->meshes.size());
                for (const auto& mesh : modelData->meshes) {
                    AABB meshAABB;
                    meshAABB.min = mesh.GetBoundsMin();
                    meshAABB.max = mesh.GetBoundsMax();
                    localAABBs_.push_back(meshAABB);
                }
                Logger::Debug("[Collision] Created {} AABBs from MeshRenderer", localAABBs_.size());
            } else {
                Logger::Debug("[Collision] Auto-sized single AABB from MeshRenderer");
            }
            return;
        }
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
    if (a.max.GetX() < b.min.GetX() || a.min.GetX() > b.max.GetX()) return false;
    if (a.max.GetY() < b.min.GetY() || a.min.GetY() > b.max.GetY()) return false;
    if (a.max.GetZ() < b.min.GetZ() || a.min.GetZ() > b.max.GetZ()) return false;
    return true;
}

Vector3 CollisionComponent::GetPenetrationVector(const AABB& a, const AABB& b) {
    // Calculate overlap on each axis
    float overlapX1 = b.max.GetX() - a.min.GetX();
    float overlapX2 = a.max.GetX() - b.min.GetX();
    float overlapY1 = b.max.GetY() - a.min.GetY();
    float overlapY2 = a.max.GetY() - b.min.GetY();
    float overlapZ1 = b.max.GetZ() - a.min.GetZ();
    float overlapZ2 = a.max.GetZ() - b.min.GetZ();

    float overlapX = (overlapX1 < overlapX2) ? overlapX1 : -overlapX2;
    float overlapY = (overlapY1 < overlapY2) ? overlapY1 : -overlapY2;
    float overlapZ = (overlapZ1 < overlapZ2) ? overlapZ1 : -overlapZ2;

    // Find minimum penetration axis
    float absX = std::abs(overlapX);
    float absY = std::abs(overlapY);
    float absZ = std::abs(overlapZ);

    if (absX <= absY && absX <= absZ) {
        return Vector3(overlapX, 0.0f, 0.0f);
    } else if (absY <= absX && absY <= absZ) {
        return Vector3(0.0f, overlapY, 0.0f);
    } else {
        return Vector3(0.0f, 0.0f, overlapZ);
    }
}

} // namespace UnoEngine
