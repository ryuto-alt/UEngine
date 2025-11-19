#pragma once

#include "../../Engine/Core/Scene.h"

#ifdef _DEBUG
#include "../UI/EditorUI.h"
#endif

namespace UnoEngine {

class GameScene : public Scene {
public:
    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RenderView& view) override;
    void OnImGui() override;

    GameObject* GetPlayer() const { return player_; }

#ifdef _DEBUG
    // EditorUIへのアクセス (Debug builds only)
    EditorUI* GetEditorUI() { return &editorUI_; }
#endif

private:
    GameObject* player_ = nullptr;

#ifdef _DEBUG
    EditorUI editorUI_;
#endif
};

} // namespace UnoEngine
