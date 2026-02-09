#include "GravitySystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"

namespace ECS {

void GravitySystem::Update(World& world, float deltaTime) {
    world.ForEach<VelocityComponent, GravityComponent>(
        [deltaTime](Entity entity, VelocityComponent& vel, GravityComponent& grav) {
            if (!grav.isGrounded) {
                vel.velocity.y += grav.gravity * deltaTime;
            }
        }
    );
}

} // namespace ECS
