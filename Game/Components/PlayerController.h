#pragma once

#include "../../Engine/Core/Component.h"
#include "../../Engine/Math/Vector.h"

namespace UnoEngine {
class InputManager;
}

namespace UnoEngine {

class PlayerController : public Component {
public:
    PlayerController() = default;

    void OnUpdate(float deltaTime) override;
    
    void SetInputManager(InputManager* input) { input_ = input; }
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    float GetMoveSpeed() const { return moveSpeed_; }

private:
    InputManager* input_ = nullptr;
    float moveSpeed_ = 5.0f;
};

} // namespace UnoEngine
