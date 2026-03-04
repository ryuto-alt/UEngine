#pragma once

#include "../Math/Vector.h"
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>

namespace UnoEngine {

class GameObject;

struct RayHit {
    GameObject* object = nullptr;
    float distance     = 0.0f;
    Vector3 point;
    Vector3 normal;
};

class PhysicsWorld {
public:
    // layerMask=0xFFFFFFFF tests all layers (uses CollisionComponent::collisionLayer)
    static std::optional<RayHit> Raycast(
        const Vector3& origin,
        const Vector3& direction,
        float maxDistance,
        uint32_t layerMask,
        const std::vector<std::unique_ptr<GameObject>>& objects);
};

} // namespace UnoEngine
