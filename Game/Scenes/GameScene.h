#pragma once

#include "../../Engine/Core/Scene.h"

namespace UnoEngine {

class GameScene : public Scene {
public:
    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RenderView& view) override;
    void OnImGui();

    GameObject* GetPlayer() const { return player_; }

private:
    GameObject* player_ = nullptr;
};

} // namespace UnoEngine
