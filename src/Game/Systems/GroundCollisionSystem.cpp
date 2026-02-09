#include "GroundCollisionSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"

namespace ECS {

void GroundCollisionSystem::Update(World& world, float deltaTime) {
    world.ForEach<TransformComponent, VelocityComponent, GravityComponent>(
        [](Entity entity, TransformComponent& transform, VelocityComponent& vel, GravityComponent& grav) {
            constexpr float groundHeight = 0.0f;
            constexpr float groundCheckOffset = 0.1f;

            if (transform.position.y <= groundHeight + groundCheckOffset) {
                transform.position.y = groundHeight;
                vel.velocity.y = 0.0f;
                grav.isGrounded = true;
            } else {
                grav.isGrounded = false;
            }
        }
    );
}

} // namespace ECS
