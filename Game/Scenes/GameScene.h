#pragma once

#include "../../Engine/Core/Scene.h"
#include "../UI/EditorUI.h"

namespace UnoEngine {

class GameScene : public Scene {
public:
    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RenderView& view) override;
    void OnImGui() override;

    GameObject* GetPlayer() const { return player_; }

    // EditorUIへのアクセス
    EditorUI* GetEditorUI() { return &editorUI_; }

private:
    GameObject* player_ = nullptr;
    EditorUI editorUI_;
};

} // namespace UnoEngine
