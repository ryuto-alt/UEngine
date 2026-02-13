#include "InputSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/CameraComponents.h"
#include "UnoEngine.h"
#include <cmath>

namespace ECS {

void InputSystem::Update(World& world, float deltaTime) {
    UnoEngine* engine = UnoEngine::GetInstance();

    world.ForEach<PlayerTag, PlayerMovementComponent, PlayerInputComponent, JumpscareVictimComponent>(
        [engine, deltaTime](Entity entity, PlayerTag&, PlayerMovementComponent& movement,
                           PlayerInputComponent& input, JumpscareVictimComponent& jumpscare) {
            if (jumpscare.isInJumpscare) return;

            // Sneak toggle (1 key or RShift)
            if (engine->IsKeyTrig(DIK_1) || engine->IsKeyTrig(DIK_RSHIFT)) {
                movement.isSneaking = !movement.isSneaking;
            }

            // Run (LShift held)
            movement.isRunning = engine->IsKeyDown(DIK_LSHIFT);

            // Gamepad B button for sneak toggle
            bool bButtonPressed = engine->IsXboxDown(0x2000);
            bool bButtonTriggered = bButtonPressed && !input.previousBButtonPressed;
            if (bButtonTriggered && movement.isMoving) {
                movement.isSneaking = !movement.isSneaking;
            }
            input.previousBButtonPressed = bButtonPressed;

            // Movement input (keyboard + gamepad)
            float forward = 0.0f;
            float right = 0.0f;

            if (engine->IsKeyDown(DIK_W)) forward += 1.0f;
            if (engine->IsKeyDown(DIK_S)) forward -= 1.0f;
            if (engine->IsKeyDown(DIK_A)) right -= 1.0f;
            if (engine->IsKeyDown(DIK_D)) right += 1.0f;

            float stickX = engine->GetLStickX();
            float stickY = engine->GetLStickY();
            constexpr float deadZone = 0.1f;
            if (std::abs(stickX) < deadZone) stickX = 0.0f;
            if (std::abs(stickY) < deadZone) stickY = 0.0f;

            forward += stickY;
            right += stickX;

            // Normalize
            float magnitude = std::sqrt(forward * forward + right * right);
            if (magnitude > 1.0f) {
                forward /= magnitude;
                right /= magnitude;
            }

            movement.moveDirection = {forward, 0.0f, right};
            movement.isMoving = (std::abs(forward) > 0.01f || std::abs(right) > 0.01f);
        }
    );
}

} // namespace ECS
