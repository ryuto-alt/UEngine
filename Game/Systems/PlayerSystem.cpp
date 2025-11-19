#include "PlayerSystem.h"
#include "../../Engine/Core/Scene.h"
#include "../Components/Player.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Input/InputManager.h"
#include "../../Engine/Math/Vector.h"

namespace UnoEngine {

void PlayerSystem::Update(Camera* camera, Player* player, InputManager* input, float deltaTime) {
    if (!input || !camera || !player) return;

    auto& keyboard = input->GetKeyboard();
    Vector3 movement = Vector3::Zero();

    if (keyboard.IsDown(KeyCode::W)) movement.SetZ(movement.GetZ() - 1.0f);
    if (keyboard.IsDown(KeyCode::S)) movement.SetZ(movement.GetZ() + 1.0f);
    if (keyboard.IsDown(KeyCode::A)) movement.SetX(movement.GetX() + 1.0f);
    if (keyboard.IsDown(KeyCode::D)) movement.SetX(movement.GetX() - 1.0f);
    if (keyboard.IsDown(KeyCode::Space)) movement.SetY(movement.GetY() + 1.0f);
    if (keyboard.IsDown(KeyCode::Control)) movement.SetY(movement.GetY() - 1.0f);

    if (movement.LengthSq() > 0.0f) {
        movement = movement.Normalize() * player->moveSpeed * deltaTime;
        camera->SetPosition(camera->GetPosition() + movement);
    }
}

void PlayerSystem::OnUpdate(Scene* scene, float deltaTime) {
    // Scene-based system implementation can be added here if needed
}

} // namespace UnoEngine
