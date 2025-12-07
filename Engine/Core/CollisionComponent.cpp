#include "pch.h"
#include "CollisionComponent.h"
#include "GameObject.h"
#include "Transform.h"
#include "../Graphics/MeshRenderer.h"
#include "../Resource/StaticModelImporter.h"
#include "../Rendering/SkinnedMeshRenderer.h"
#include "Logger.h"

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

AABB CollisionComponent::GetWorldAABB() const {
    if (!GetGameObject()) {
        return localAABB_;
    }

    auto& transform = GetGameObject()->GetTransform();
    Vector3 worldPos = transform.GetPosition();
    Vector3 worldScale = transform.GetScale();

    Vector3 scaledMin(
        localAABB_.min.GetX() * worldScale.GetX(),
        localAABB_.min.GetY() * worldScale.GetY(),
        localAABB_.min.GetZ() * worldScale.GetZ()
    );
    Vector3 scaledMax(
        localAABB_.max.GetX() * worldScale.GetX(),
        localAABB_.max.GetY() * worldScale.GetY(),
        localAABB_.max.GetZ() * worldScale.GetZ()
    );

    // AABB doesn't rotate properly, but for basic collision this works
    // For rotated objects, we recalculate bounds to maintain axis-alignment
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

    // Ensure min < max after scaling (negative scale handling)
    if (worldAABB.min.GetX() > worldAABB.max.GetX()) {
        float temp = worldAABB.min.GetX();
        worldAABB.min = Vector3(worldAABB.max.GetX(), worldAABB.min.GetY(), worldAABB.min.GetZ());
        worldAABB.max = Vector3(temp, worldAABB.max.GetY(), worldAABB.max.GetZ());
    }
    if (worldAABB.min.GetY() > worldAABB.max.GetY()) {
        float temp = worldAABB.min.GetY();
        worldAABB.min = Vector3(worldAABB.min.GetX(), worldAABB.max.GetY(), worldAABB.min.GetZ());
        worldAABB.max = Vector3(worldAABB.max.GetX(), temp, worldAABB.max.GetZ());
    }
    if (worldAABB.min.GetZ() > worldAABB.max.GetZ()) {
        float temp = worldAABB.min.GetZ();
        worldAABB.min = Vector3(worldAABB.min.GetX(), worldAABB.min.GetY(), worldAABB.max.GetZ());
        worldAABB.max = Vector3(worldAABB.max.GetX(), worldAABB.max.GetY(), temp);
    }

    return worldAABB;
}

void CollisionComponent::SetLocalAABB(const Vector3& min, const Vector3& max) {
    localAABB_.min = min;
    localAABB_.max = max;
    useAutoSize_ = false;
}

void CollisionComponent::RecalculateFromMesh() {
    if (!GetGameObject()) {
        return;
    }

    // Try SkinnedMeshRenderer first
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

    // Try MeshRenderer
    auto* meshRenderer = GetGameObject()->GetComponent<MeshRenderer>();
    if (meshRenderer && meshRenderer->HasModel()) {
        auto* modelData = meshRenderer->GetModel();
        if (modelData) {
            localAABB_.min = modelData->boundingBox.min;
            localAABB_.max = modelData->boundingBox.max;
            Logger::Debug("[Collision] Auto-sized AABB from MeshRenderer");
            return;
        }
    }

    // Default unit cube if no mesh found
    localAABB_.min = Vector3(-0.5f, -0.5f, -0.5f);
    localAABB_.max = Vector3(0.5f, 0.5f, 0.5f);
}

bool CollisionComponent::CheckAABBCollision(const AABB& a, const AABB& b) {
    // AABB vs AABB intersection test
    // Two AABBs intersect if they overlap on all three axes
    if (a.max.GetX() < b.min.GetX() || a.min.GetX() > b.max.GetX()) return false;
    if (a.max.GetY() < b.min.GetY() || a.min.GetY() > b.max.GetY()) return false;
    if (a.max.GetZ() < b.min.GetZ() || a.min.GetZ() > b.max.GetZ()) return false;
    return true;
}

} // namespace UnoEngine
