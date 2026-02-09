#pragma once
#include "AABBCollision.h"
#include <string>

namespace ECS {

struct AABBColliderComponent {
    Collision::AABB localAABB;
    Collision::AABB worldAABB;
    bool enabled = true;
    std::string name;
};

} // namespace ECS
