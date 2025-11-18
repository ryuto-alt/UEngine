#include "PlayerController.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/SceneManager.h"
#include "../../Engine/Input/InputManager.h"

namespace UnoEngine {

	void PlayerController::OnUpdate(float deltaTime) {
		auto* input = SceneManager::GetInstance().GetInputManager();
		if (!input) return;

		auto& keyboard = input->GetKeyboard();
		Vector3 movement = Vector3::Zero();

		// WASD movement
		if (keyboard.IsDown(KeyCode::W)) {
			movement.SetZ(movement.GetZ() + 1.0f);
		}
		if (keyboard.IsDown(KeyCode::S)) {
			movement.SetZ(movement.GetZ() - 1.0f);
		}
		if (keyboard.IsDown(KeyCode::A)) {
			movement.SetX(movement.GetX() - 1.0f);
		}
		if (keyboard.IsDown(KeyCode::D)) {
			movement.SetX(movement.GetX() + 1.0f);
		}
		if (keyboard.IsDown(KeyCode::Space)) {
			movement.SetY(movement.GetY() - 1.0f);
		}
		if (keyboard.IsDown(KeyCode::Control)) {
			movement.SetY(movement.GetY() + 1.0f);
		}

		// Apply movement
		if (movement.LengthSq() > 0.0f) {
			movement = movement.Normalize() * moveSpeed_ * deltaTime;
			auto& transform = GetGameObject()->GetTransform();
			transform.SetLocalPosition(transform.GetLocalPosition() + movement);
		}
	}

} // namespace UnoEngine
