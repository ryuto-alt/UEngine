#include "VelocityIntegrationSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"

namespace ECS {

void VelocityIntegrationSystem::Update(World& world, float deltaTime) {
    world.ForEach<TransformComponent, VelocityComponent>(
        [deltaTime](Entity entity, TransformComponent& transform, VelocityComponent& vel) {
            transform.position.x += vel.velocity.x * deltaTime;
            transform.position.y += vel.velocity.y * deltaTime;
            transform.position.z += vel.velocity.z * deltaTime;
        }
    );
}

} // namespace ECS
