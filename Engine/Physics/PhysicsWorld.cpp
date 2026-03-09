#include "pch.h"
#include "PhysicsWorld.h"
#include "../Core/GameObject.h"
#include "../Core/CollisionComponent.h"
#include "MeshColliderComponent.h"
#include "../Core/Transform.h"
#include "../Math/Matrix.h"
#include <limits>
#include <cmath>

namespace UnoEngine {

namespace {

// Slab method ray-AABB intersection. Returns true and sets tMin/hitNormal.
bool RayAABBIntersect(const Vector3& origin, const Vector3& dir,
                      const AABB& aabb, float& tMin, Vector3& hitNormal) {
    tMin          = 0.0f;
    float tMax    = std::numeric_limits<float>::max();
    hitNormal     = Vector3(0, 0, 0);
    Vector3 enterNormal;

    float ox[3] = { origin.GetX(), origin.GetY(), origin.GetZ() };
    float dx[3] = { dir.GetX(),    dir.GetY(),    dir.GetZ()    };
    float mn[3] = { aabb.min.GetX(), aabb.min.GetY(), aabb.min.GetZ() };
    float mx[3] = { aabb.max.GetX(), aabb.max.GetY(), aabb.max.GetZ() };

    for (int i = 0; i < 3; ++i) {
        if (std::abs(dx[i]) < 1e-8f) {
            if (ox[i] < mn[i] || ox[i] > mx[i]) return false;
            continue;
        }

        float t1 = (mn[i] - ox[i]) / dx[i];
        float t2 = (mx[i] - ox[i]) / dx[i];

        float enterN[3] = {0,0,0};
        if (t1 > t2) {
            std::swap(t1, t2);
            enterN[i] = dx[i] > 0 ? 1.0f : -1.0f;
        } else {
            enterN[i] = dx[i] < 0 ? 1.0f : -1.0f;
        }

        if (t1 > tMin) {
            tMin        = t1;
            enterNormal = Vector3(enterN[0], enterN[1], enterN[2]);
        }
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    if (tMin < 0.0f) return false;
    hitNormal = enterNormal;
    return true;
}

} // anonymous namespace

std::optional<RayHit> PhysicsWorld::Raycast(
    const Vector3& origin,
    const Vector3& direction,
    float maxDistance,
    uint32_t layerMask,
    const std::vector<std::unique_ptr<GameObject>>& objects)
{
    Vector3 dir = direction.Normalize();
    std::optional<RayHit> closest;
    float closestDist = maxDistance;

    for (const auto& obj : objects) {
        if (!obj || !obj->IsActive()) continue;

        auto* collision = obj->GetComponent<CollisionComponent>();
        if (!collision || !collision->IsEnabled()) continue;

        // Layer filtering via CollisionComponent layer
        if ((collision->GetCollisionLayer() & layerMask) == 0) continue;

        auto testAABB = [&](const AABB& aabb) {
            float t;
            Vector3 normal;
            if (RayAABBIntersect(origin, dir, aabb, t, normal) && t < closestDist) {
                closestDist = t;
                RayHit hit;
                hit.object   = obj.get();
                hit.distance = t;
                hit.point    = origin + dir * t;
                hit.normal   = normal;
                closest = hit;
            }
        };

        if (collision->HasMultipleAABBs()) {
            for (const auto& aabb : collision->GetWorldAABBs()) testAABB(aabb);
        } else {
            testAABB(collision->GetWorldAABB());
        }

        // BVH mesh raycast for higher precision
        auto* meshCollider = obj->GetComponent<MeshColliderComponent>();
        if (meshCollider && meshCollider->IsBuilt()) {
            Matrix4x4 worldMatrix = obj->GetTransform().GetWorldMatrix();
            Matrix4x4 invWorld = worldMatrix.Inverse();

            Vector3 localOrigin = invWorld.TransformPoint(origin);
            Vector3 localDir = invWorld.TransformDirection(dir).Normalize();

            auto bvhHit = meshCollider->GetBVH()->Raycast(localOrigin, localDir, closestDist);
            if (bvhHit) {
                // Transform hit back to world space
                Vector3 worldPoint = worldMatrix.TransformPoint(bvhHit->point);
                float worldDist = (worldPoint - origin).Length();
                if (worldDist < closestDist) {
                    closestDist = worldDist;
                    RayHit hit;
                    hit.object   = obj.get();
                    hit.distance = worldDist;
                    hit.point    = worldPoint;
                    hit.normal   = worldMatrix.TransformDirection(bvhHit->normal).Normalize();
                    closest = hit;
                }
            }
        }
    }

    return closest;
}

} // namespace UnoEngine
