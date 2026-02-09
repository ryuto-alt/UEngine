#include "RotationSmoothingSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include <cmath>

namespace ECS {

void RotationSmoothingSystem::Update(World& world, float deltaTime) {
    world.ForEach<TransformComponent, RotationSmoothingComponent>(
        [deltaTime](Entity entity, TransformComponent& transform, RotationSmoothingComponent& rot) {
            // Skip entities with external rotation control (e.g., enemy uses PathfindingSystem)
            if (rot.externalControl) return;

            // Shortest-path angle interpolation
            float diff = rot.targetRotationY - rot.currentRotationY;

            // Normalize to [-PI, PI]
            constexpr float PI = 3.14159265358979323846f;
            while (diff > PI) diff -= 2.0f * PI;
            while (diff < -PI) diff += 2.0f * PI;

            float t = 1.0f - std::exp(-rot.smoothingSpeed * deltaTime);
            rot.currentRotationY += diff * t;

            // Normalize result
            while (rot.currentRotationY > PI) rot.currentRotationY -= 2.0f * PI;
            while (rot.currentRotationY < -PI) rot.currentRotationY += 2.0f * PI;

            transform.rotation.y = rot.currentRotationY;
        }
    );
}

} // namespace ECS
