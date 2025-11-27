#pragma once

#include "../../Engine/Systems/ISystem.h"
#include "../../Engine/Core/GameObject.h"

namespace UnoEngine {

class InputManager;
class Camera;
struct Player;

class CameraSystem : public ISystem {
public:
    void OnUpdate(Scene* scene, float deltaTime) override;
    int GetPriority() const override { return 50; }
    
    void Update(Camera* camera, Player* player, InputManager* input, float deltaTime);
};

} // namespace UnoEngine
