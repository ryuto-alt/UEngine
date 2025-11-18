#pragma once

#include "../../Engine/Core/Scene.h"

namespace UnoEngine {
class InputManager;
}

namespace UnoEngine {

class GameScene : public Scene {
public:
    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RenderView& view) override;
    
    void SetInputManager(InputManager* input) { input_ = input; }
    
private:
    InputManager* input_ = nullptr;
};

} // namespace UnoEngine
