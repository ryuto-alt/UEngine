#include "PlayerMovementSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "UnoEngine.h"
#include <cmath>

namespace ECS {

void PlayerMovementSystem::Update(World& world, float deltaTime) {
    Camera* camera = world.GetResource<Camera*>();

    world.ForEach<PlayerTag, TransformComponent, PlayerMovementComponent, RotationSmoothingComponent, JumpscareVictimComponent>(
        [camera, deltaTime](Entity entity, PlayerTag&, TransformComponent& transform,
                           PlayerMovementComponent& movement, RotationSmoothingComponent& rot,
                           JumpscareVictimComponent& jumpscare) {
            if (!camera || !movement.isMoving || jumpscare.isInJumpscare) return;

            float forward = movement.moveDirection.x; // W/S + stickY
            float right = movement.moveDirection.z;    // A/D + stickX

            Vector3 cameraForward = camera->GetForwardVector();
            Vector3 cameraRight = camera->GetRightVector();

            // Project to XZ plane
            cameraForward.y = 0.0f;
            cameraRight.y = 0.0f;

            float fwdLen = std::sqrt(cameraForward.x * cameraForward.x + cameraForward.z * cameraForward.z);
            float rgtLen = std::sqrt(cameraRight.x * cameraRight.x + cameraRight.z * cameraRight.z);

            if (fwdLen > 0.0f) { cameraForward.x /= fwdLen; cameraForward.z /= fwdLen; }
            if (rgtLen > 0.0f) { cameraRight.x /= rgtLen; cameraRight.z /= rgtLen; }

            Vector3 moveDir = {
                cameraForward.x * forward + cameraRight.x * right,
                0.0f,
                cameraForward.z * forward + cameraRight.z * right
            };

            float moveLen = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
            if (moveLen > 0.0f) {
                moveDir.x /= moveLen;
                moveDir.z /= moveLen;

                float currentSpeed = movement.moveSpeed;
                if (movement.isSneaking) {
                    currentSpeed *= movement.sneakSpeedMultiplier;
                } else if (movement.isSprinting) {
                    currentSpeed *= SprintComponent::kSpeedMultiplier;
                } else if (movement.isRunning) {
                    currentSpeed *= movement.runSpeedMultiplier;
                }

                float distance = currentSpeed * deltaTime;
                transform.position.x += moveDir.x * distance;
                transform.position.z += moveDir.z * distance;

                rot.targetRotationY = std::atan2(moveDir.x, moveDir.z);
            }
        }
    );
}

} // namespace ECS
