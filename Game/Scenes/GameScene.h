#pragma once

#include "../../Engine/Core/Scene.h"
#include <vector>
#include <string>

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
    EditorUI* GetEditorUI() { return &editorUI_; }
#endif

private:
    void SetupCamera();
    void SetupLighting();
    void SetupPlayer();
    void SetupAnimatedCharacter();

    GameObject* player_ = nullptr;
    GameObject* animatedCharacter_ = nullptr;

    // Model path for editor display
    std::string loadedModelPath_;

#ifdef _DEBUG
    EditorUI editorUI_;
#endif
};

} // namespace UnoEngine
