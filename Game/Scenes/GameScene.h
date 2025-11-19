#pragma once

#include "../../Engine/Core/Scene.h"
#include "../../Engine/Graphics/RenderTexture.h"
#include <vector>
#include <string>

namespace UnoEngine {

class GameScene : public Scene {
public:
    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RenderView& view) override;
    void OnImGui() override;

    GameObject* GetPlayer() const { return player_; }

    RenderTexture* GetGameViewTexture() { return &gameViewTexture_; }
    RenderTexture* GetSceneViewTexture() { return &sceneViewTexture_; }

private:
    GameObject* player_ = nullptr;
    GameObject* selectedObject_ = nullptr;

    RenderTexture gameViewTexture_;
    RenderTexture sceneViewTexture_;

    // 次のフレームで適用するサイズ
    uint32 desiredGameViewWidth_ = 1280;
    uint32 desiredGameViewHeight_ = 720;
    uint32 desiredSceneViewWidth_ = 1280;
    uint32 desiredSceneViewHeight_ = 720;

    bool showGameView_ = true;
    bool showSceneView_ = true;
    bool showInspector_ = true;
    bool showHierarchy_ = true;
    bool showStats_ = true;
    bool showConsole_ = true;
    bool showProject_ = true;
    bool showProfiler_ = false;

    // Console log messages
    std::vector<std::string> consoleMessages_;
};

} // namespace UnoEngine
