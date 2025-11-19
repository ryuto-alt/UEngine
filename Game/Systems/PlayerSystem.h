#pragma once

#include "../../Engine/Core/GameObject.h"

namespace UnoEngine {

class InputManager;
class Camera;
struct Player;

class PlayerSystem {
public:
    void Update(Camera* camera, Player* player, InputManager* input, float deltaTime);
};

} // namespace UnoEngine
