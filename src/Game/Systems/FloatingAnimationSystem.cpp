#include "FloatingAnimationSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/CollectibleComponents.h"
#include <cmath>

namespace ECS {

void FloatingAnimationSystem::Update(World& world, float deltaTime) {
    world.ForEach<TransformComponent, FloatingAnimationComponent, OrbComponent>(
        [deltaTime](Entity entity, TransformComponent& transform,
                    FloatingAnimationComponent& floatAnim, OrbComponent& orb) {
            if (orb.isCollected) return;

            floatAnim.floatTimer += floatAnim.floatSpeed * deltaTime;
            float offset = std::sin(floatAnim.floatTimer) * floatAnim.floatAmplitude;

            transform.position = floatAnim.basePosition;
            transform.position.y += offset;
        }
    );

    // Rotation for collectibles
    world.ForEach<TransformComponent, RotatingComponent, OrbComponent>(
        [deltaTime](Entity entity, TransformComponent& transform,
                    RotatingComponent& rotating, OrbComponent& orb) {
            if (orb.isCollected) return;
            transform.rotation.y += rotating.rotationSpeed * deltaTime;
        }
    );
}

} // namespace ECS
