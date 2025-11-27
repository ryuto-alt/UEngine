#pragma once

#include "../../Engine/Graphics/RenderTexture.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Core/Types.h"
#include <vector>
#include <string>

namespace UnoEngine {

class GraphicsDevice;

// EditorUIに渡す情報をまとめた構造体
struct EditorContext {
    GameObject* player = nullptr;
    Camera* camera = nullptr;
    const std::vector<UniquePtr<GameObject>>* gameObjects = nullptr;
    float fps = 0.0f;
    float frameTime = 0.0f;

    // ロードされたアセット情報
    std::vector<std::string> loadedModels;
    std::vector<std::string> loadedTextures;
    std::string currentSceneName;
};

// Editor UI管理クラス
class EditorUI {
public:
    EditorUI() = default;
    ~EditorUI() = default;

    // 初期化
    void Initialize(GraphicsDevice* graphics);

    // ImGuiレンダリング
    void Render(const EditorContext& context);

    // RenderTextureのアクセサ
    RenderTexture* GetGameViewTexture() { return &gameViewTexture_; }
    RenderTexture* GetSceneViewTexture() { return &sceneViewTexture_; }

    // 次のフレームで適用するビューポートサイズを取得
    void GetDesiredViewportSizes(uint32& gameW, uint32& gameH, uint32& sceneW, uint32& sceneH) const {
        gameW = desiredGameViewWidth_;
        gameH = desiredGameViewHeight_;
        sceneW = desiredSceneViewWidth_;
        sceneH = desiredSceneViewHeight_;
    }

    // コンソールにメッセージを追加
    void AddConsoleMessage(const std::string& message) {
        consoleMessages_.push_back(message);
    }

private:
    // 各パネルの描画メソッド
    void RenderDockSpace();
    void RenderGameView();
    void RenderSceneView();
    void RenderInspector(const EditorContext& context);
    void RenderHierarchy(const EditorContext& context);
    void RenderStats(const EditorContext& context);
    void RenderConsole();
    void RenderProject(const EditorContext& context);
    void RenderProfiler();

private:
    // RenderTexture
    RenderTexture gameViewTexture_;
    RenderTexture sceneViewTexture_;

    // 次のフレームで適用するサイズ
    uint32 desiredGameViewWidth_ = 1280;
    uint32 desiredGameViewHeight_ = 720;
    uint32 desiredSceneViewWidth_ = 1280;
    uint32 desiredSceneViewHeight_ = 720;

    // パネル表示状態
    bool showGameView_ = true;
    bool showSceneView_ = true;
    bool showInspector_ = true;
    bool showHierarchy_ = true;
    bool showStats_ = true;
    bool showConsole_ = true;
    bool showProject_ = true;
    bool showProfiler_ = false;

    // Docking layout
    bool dockingLayoutInitialized_ = false;

    // Console log messages
    std::vector<std::string> consoleMessages_;

    // 選択中のオブジェクト
    GameObject* selectedObject_ = nullptr;
};

} // namespace UnoEngine
