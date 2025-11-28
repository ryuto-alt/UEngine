#pragma once

#include "../../Engine/Graphics/RenderTexture.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Core/Types.h"
#include "EditorCamera.h"
#include "GizmoSystem.h"
#include <vector>
#include <string>

namespace UnoEngine {

class GraphicsDevice;
class DebugRenderer;
class AnimationSystem;

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

    // デバッグ表示設定
    DebugRenderer* debugRenderer = nullptr;
    AnimationSystem* animationSystem = nullptr;
};

// エディタモード
enum class EditorMode {
    Edit,   // 編集モード（シーン編集可能）
    Play,   // 再生モード（ゲーム実行中）
    Pause   // 一時停止（再生中だが停止）
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

    // 描画が必要なViewかどうか
    bool ShouldRenderSceneView() const { return showSceneView_; }
    bool ShouldRenderGameView() const { return showGameView_; }

    // エディタモード
    EditorMode GetEditorMode() const { return editorMode_; }
    bool IsPlaying() const { return editorMode_ == EditorMode::Play; }
    bool IsPaused() const { return editorMode_ == EditorMode::Pause; }
    bool IsEditing() const { return editorMode_ == EditorMode::Edit; }
    
    void Play();
    void Pause();
    void Stop();
    void Step();  // 1フレーム進める

    // アニメーションシステムとの連携
    void SetAnimationSystem(AnimationSystem* animSystem) { animationSystem_ = animSystem; }

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

    // エディタカメラ
    void SetEditorCamera(Camera* camera) { editorCamera_.SetCamera(camera); }
    EditorCamera& GetEditorCamera() { return editorCamera_; }

    // ギズモシステム
    GizmoSystem& GetGizmoSystem() { return gizmoSystem_; }

    // オブジェクト選択
    void SetSelectedObject(GameObject* obj) { selectedObject_ = obj; }
    GameObject* GetSelectedObject() const { return selectedObject_; }

private:
    // 各パネルの描画メソッド
    void RenderDockSpace();
    void RenderSceneView();
    void RenderGameView();
    void RenderInspector(const EditorContext& context);
    void RenderHierarchy(const EditorContext& context);
    void RenderStats(const EditorContext& context);
    void RenderConsole();
    void RenderProject(const EditorContext& context);
    void RenderProfiler();

    // ホットキー処理
    void ProcessHotkeys();

private:
    // RenderTexture
    RenderTexture gameViewTexture_;
    RenderTexture sceneViewTexture_;

    // 次のフレームで適用するサイズ
    uint32 desiredGameViewWidth_ = 1280;
    uint32 desiredGameViewHeight_ = 720;
    uint32 desiredSceneViewWidth_ = 1280;
    uint32 desiredSceneViewHeight_ = 720;

    // View表示状態
    bool showSceneView_ = true;
    bool showGameView_ = true;

    // Play/Edit モード
#ifdef NDEBUG
    EditorMode editorMode_ = EditorMode::Play;  // Releaseでは自動再生
#else
    EditorMode editorMode_ = EditorMode::Edit;  // Debugでは編集モード
#endif
    bool stepFrame_ = false;  // 1フレームだけ進める

    // パネル表示状態
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

    // エディタカメラ
    EditorCamera editorCamera_;

    // ギズモシステム
    GizmoSystem gizmoSystem_;

    // Scene Viewの位置・サイズ（ギズモ用）
    float sceneViewPosX_ = 0.0f;
    float sceneViewPosY_ = 0.0f;
    float sceneViewSizeX_ = 0.0f;
    float sceneViewSizeY_ = 0.0f;

    // アニメーションシステム参照
    AnimationSystem* animationSystem_ = nullptr;
};

} // namespace UnoEngine
