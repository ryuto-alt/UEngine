#include "EditorUI.h"
#include "../../Engine/Graphics/GraphicsDevice.h"
#include "../../Engine/Rendering/DebugRenderer.h"
#include "../../Engine/Animation/AnimationSystem.h"
#include "../../Engine/Scene/SceneSerializer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "../../Engine/UI/imgui_toggle.h"
#include "../../Engine/UI/imgui_toggle_presets.h"
#include "ImGuizmo.h"

namespace UnoEngine {

void EditorUI::Initialize(GraphicsDevice* graphics) {
    // RenderTexture setup (SRVインデックス 3と4を使用) - 16:9 aspect ratio
    gameViewTexture_.Create(graphics, 1280, 720, 3);
    sceneViewTexture_.Create(graphics, 1280, 720, 4);

    // ギズモシステム初期化
    gizmoSystem_.Initialize();

    // Console初期ログ
    consoleMessages_.push_back("[System] UnoEngine Editor Initialized");
    consoleMessages_.push_back("[Info] Press ~ to toggle console");
    consoleMessages_.push_back("[Info] Q: Translate, E: Rotate, R: Scale");
}

void EditorUI::Render(const EditorContext& context) {
    // ImGuizmoフレーム開始
    ImGuizmo::BeginFrame();

    // カメラを設定（毎フレーム）
    if (context.camera) {
        editorCamera_.SetCamera(context.camera);
    }

    // アニメーションシステムを設定
    if (context.animationSystem) {
        animationSystem_ = context.animationSystem;
    }

    // ホットキー処理
    ProcessHotkeys();

    RenderDockSpace();
    RenderSceneView();
    RenderGameView();
    RenderInspector(context);
    RenderHierarchy(context);
    RenderStats(context);
    RenderConsole();
    RenderProject(context);
    RenderProfiler();

    // エディタカメラの更新（Edit/Pauseモードのみ）
    if (editorMode_ != EditorMode::Play) {
        float deltaTime = ImGui::GetIO().DeltaTime;
        editorCamera_.Update(deltaTime);
    }

    // ステップフレームをリセット
    stepFrame_ = false;
}

void EditorUI::Play() {
    if (editorMode_ == EditorMode::Edit) {
        editorMode_ = EditorMode::Play;
        // アニメーション再生開始
        if (animationSystem_) {
            animationSystem_->SetPlaying(true);
        }
        consoleMessages_.push_back("[Editor] Play mode started");
    } else if (editorMode_ == EditorMode::Pause) {
        editorMode_ = EditorMode::Play;
        // アニメーション再開
        if (animationSystem_) {
            animationSystem_->SetPlaying(true);
        }
        consoleMessages_.push_back("[Editor] Resumed");
    }
}

void EditorUI::Pause() {
    if (editorMode_ == EditorMode::Play) {
        editorMode_ = EditorMode::Pause;
        // アニメーション一時停止
        if (animationSystem_) {
            animationSystem_->SetPlaying(false);
        }
        consoleMessages_.push_back("[Editor] Paused");
    }
}

void EditorUI::Stop() {
    if (editorMode_ != EditorMode::Edit) {
        editorMode_ = EditorMode::Edit;
        // アニメーション停止
        if (animationSystem_) {
            animationSystem_->SetPlaying(false);
        }
        consoleMessages_.push_back("[Editor] Stopped - returned to Edit mode");
    }
}

void EditorUI::Step() {
    if (editorMode_ == EditorMode::Pause) {
        stepFrame_ = true;
        consoleMessages_.push_back("[Editor] Step frame");
    }
}

void EditorUI::RenderDockSpace() {
    // DockSpace Setup
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Menu Bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::SeparatorText("Viewports");
            ImGui::MenuItem("Scene View", "F1", &showSceneView_);
            ImGui::MenuItem("Game View", "F2", &showGameView_);

            ImGui::SeparatorText("Tools");
            ImGui::MenuItem("Inspector", nullptr, &showInspector_);
            ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy_);
            ImGui::MenuItem("Console", nullptr, &showConsole_);
            ImGui::MenuItem("Project", nullptr, &showProject_);

            ImGui::SeparatorText("Performance");
            ImGui::MenuItem("Stats", nullptr, &showStats_);
            ImGui::MenuItem("Profiler", nullptr, &showProfiler_);

            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout", "Ctrl+Shift+R")) {
                dockingLayoutInitialized_ = false;
            }

            ImGui::EndMenu();
        }

        // Play/Pause/Stop ボタンをメニューバー中央に配置
        float menuBarWidth = ImGui::GetWindowWidth();
        float buttonWidth = 28.0f;
        float totalWidth = buttonWidth * 3 + 8.0f;
        float startX = (menuBarWidth - totalWidth) * 0.5f;
        
        ImGui::SetCursorPosX(startX);
        
        bool isPlaying = (editorMode_ == EditorMode::Play);
        bool isPaused = (editorMode_ == EditorMode::Pause);
        
        // Play/Pauseボタン
        if (isPlaying) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        }
        if (ImGui::Button(isPlaying ? "||##PlayBtn" : ">##PlayBtn", ImVec2(buttonWidth, 0))) {
            if (isPlaying) {
                Pause();
            } else {
                Play();
            }
        }
        if (isPlaying) {
            ImGui::PopStyleColor();
        }
        
        ImGui::SameLine();
        
        // Stopボタン
        bool canStop = (editorMode_ != EditorMode::Edit);
        if (!canStop) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }
        if (ImGui::Button("[]##StopBtn", ImVec2(buttonWidth, 0)) && canStop) {
            Stop();
        }
        if (!canStop) {
            ImGui::PopStyleVar();
        }
        
        ImGui::SameLine();
        
        // Stepボタン
        bool canStep = isPaused;
        if (!canStep) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }
        if (ImGui::Button(">|##StepBtn", ImVec2(buttonWidth, 0)) && canStep) {
            Step();
        }
        if (!canStep) {
            ImGui::PopStyleVar();
        }
        
        // モード表示
        ImGui::SameLine();
        const char* modeText = "Edit";
        ImVec4 modeColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        if (isPlaying) {
            modeText = "Playing";
            modeColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        } else if (isPaused) {
            modeText = "Paused";
            modeColor = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
        }
        ImGui::TextColored(modeColor, "%s", modeText);

        ImGui::EndMenuBar();
    }

    // 初回起動時にデフォルトレイアウトを構築
    if (!dockingLayoutInitialized_) {
        dockingLayoutInitialized_ = true;

        // レイアウトをリセット
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

        // ドックスペースを分割
        ImGuiID dock_top, dock_bottom;
        ImGuiID dock_left, dock_right;
        ImGuiID dock_scene, dock_game;
        ImGuiID dock_project, dock_console;

        // 上(65%) | 下(35%)
        dock_top = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Up, 0.65f, nullptr, &dock_bottom);

        // 上部を左(20%) | 右(80%)に分割
        dock_left = ImGui::DockBuilderSplitNode(dock_top, ImGuiDir_Left, 0.20f, nullptr, &dock_right);

        // 右上部を左右に分割（Scene 50% | Game 50%）
        dock_scene = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Left, 0.5f, nullptr, &dock_game);

        // 下部を左右に分割（Project 20% | Console 80%）
        dock_project = ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Left, 0.20f, nullptr, &dock_console);

        // パネルをドックに配置
        // 左上: Hierarchy, Inspector, Stats, Profiler（タブ）
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_left);
        ImGui::DockBuilderDockWindow("Stats", dock_left);
        ImGui::DockBuilderDockWindow("Profiler", dock_left);

        // 中央: Scene（左）、Game（右）
        ImGui::DockBuilderDockWindow("Scene", dock_scene);
        ImGui::DockBuilderDockWindow("Game", dock_game);

        // 下部: Project（左）、Console（右）
        ImGui::DockBuilderDockWindow("Project", dock_project);
        ImGui::DockBuilderDockWindow("Console", dock_console);

        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();
}

void EditorUI::RenderSceneView() {
    if (!showSceneView_) return;

    ImGui::Begin("Scene", &showSceneView_);

    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    if (availableSize.x > 0 && availableSize.y > 0) {
        const float aspectRatio = 16.0f / 9.0f;
        ImVec2 imageSize;

        imageSize.x = availableSize.x;
        imageSize.y = availableSize.x / aspectRatio;

        if (imageSize.y > availableSize.y) {
            imageSize.y = availableSize.y;
            imageSize.x = availableSize.y * aspectRatio;
        }

        ImVec2 cursorPos = ImGui::GetCursorPos();
        cursorPos.x += (availableSize.x - imageSize.x) * 0.5f;
        cursorPos.y += (availableSize.y - imageSize.y) * 0.5f;
        ImGui::SetCursorPos(cursorPos);

        desiredSceneViewWidth_ = static_cast<uint32>(imageSize.x);
        desiredSceneViewHeight_ = static_cast<uint32>(imageSize.y);

        ImGui::Image((ImTextureID)sceneViewTexture_.GetSRVHandle().ptr, imageSize);

        // Scene View画像のホバー状態でカメラ操作を有効化
        bool sceneHovered = ImGui::IsItemHovered();
        editorCamera_.SetViewportHovered(sceneHovered);
        editorCamera_.SetViewportFocused(ImGui::IsWindowFocused());

        // 画像の実際のスクリーン位置を計算（ギズモ用）
        // GetItemRectMin()で直前に描画したImage の正確なスクリーン座標を取得
        ImVec2 imageMin = ImGui::GetItemRectMin();
        ImVec2 imageMax = ImGui::GetItemRectMax();
        sceneViewPosX_ = imageMin.x;
        sceneViewPosY_ = imageMin.y;
        sceneViewSizeX_ = imageMax.x - imageMin.x;
        sceneViewSizeY_ = imageMax.y - imageMin.y;

        // エディタカメラにビューポート矩形を設定（マウスクリップ用）
        editorCamera_.SetViewportRect(sceneViewPosX_, sceneViewPosY_, sceneViewSizeX_, sceneViewSizeY_);

        // ギズモ描画（Edit/Pauseモードかつオブジェクトが選択されている場合）
        if (editorMode_ != EditorMode::Play && selectedObject_ && editorCamera_.GetCamera()) {
            // ギズモ操作開始時にスナップショットを保存
            if (gizmoSystem_.IsUsing() && !isGizmoActive_) {
                isGizmoActive_ = true;
                auto& transform = selectedObject_->GetTransform();
                preGizmoSnapshot_.targetObject = selectedObject_;
                preGizmoSnapshot_.position = transform.GetLocalPosition();
                preGizmoSnapshot_.rotation = transform.GetLocalRotation();
                preGizmoSnapshot_.scale = transform.GetLocalScale();
            }

            bool manipulated = gizmoSystem_.RenderGizmo(
                selectedObject_,
                editorCamera_.GetCamera(),
                sceneViewPosX_,
                sceneViewPosY_,
                sceneViewSizeX_,
                sceneViewSizeY_
            );

            // ギズモ操作終了時に履歴に追加
            if (!gizmoSystem_.IsUsing() && isGizmoActive_) {
                isGizmoActive_ = false;
                PushUndoSnapshot(preGizmoSnapshot_);
            }
        }
    }

    ImGui::End();
}

void EditorUI::RenderGameView() {
    if (!showGameView_) return;

    ImGui::Begin("Game", &showGameView_);

    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    if (availableSize.x > 0 && availableSize.y > 0) {
        const float aspectRatio = 16.0f / 9.0f;
        ImVec2 imageSize;

        imageSize.x = availableSize.x;
        imageSize.y = availableSize.x / aspectRatio;

        if (imageSize.y > availableSize.y) {
            imageSize.y = availableSize.y;
            imageSize.x = availableSize.y * aspectRatio;
        }

        ImVec2 cursorPos = ImGui::GetCursorPos();
        cursorPos.x += (availableSize.x - imageSize.x) * 0.5f;
        cursorPos.y += (availableSize.y - imageSize.y) * 0.5f;
        ImGui::SetCursorPos(cursorPos);

        desiredGameViewWidth_ = static_cast<uint32>(imageSize.x);
        desiredGameViewHeight_ = static_cast<uint32>(imageSize.y);

        ImGui::Image((ImTextureID)gameViewTexture_.GetSRVHandle().ptr, imageSize);
    }

    ImGui::End();
}

void EditorUI::RenderInspector(const EditorContext& context) {
    if (!showInspector_) return;

    ImGui::Begin("Inspector", &showInspector_);

    if (context.player) {
        ImGui::Text("Selected: Player");
        ImGui::Separator();

        auto& transform = context.player->GetTransform();
        auto pos = transform.GetLocalPosition();
        auto rot = transform.GetLocalRotation();
        auto scale = transform.GetLocalScale();

        ImGui::Text("Transform");
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.GetX(), pos.GetY(), pos.GetZ());
        ImGui::Text("Rotation: (%.2f, %.2f, %.2f, %.2f)",
            rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());
        ImGui::Text("Scale: (%.2f, %.2f, %.2f)", scale.GetX(), scale.GetY(), scale.GetZ());
    } else {
        ImGui::Text("No object selected");
    }

    ImGui::Separator();
    ImGui::Text("Debug Settings");
    ImGui::Spacing();

    ImGuiToggleConfig config = ImGuiTogglePresets::MaterialStyle(1.0f);

    // Animation Toggle
    if (context.animationSystem) {
        bool isPlaying = context.animationSystem->IsPlaying();
        ImGui::Text("Animation");
        ImGui::SameLine(100.0f);
        if (ImGui::Toggle("##AnimToggle", &isPlaying, config)) {
            context.animationSystem->SetPlaying(isPlaying);
        }
    }

    // Debug Bones Toggle
    if (context.debugRenderer) {
        bool showBones = context.debugRenderer->GetShowBones();
        ImGui::Text("Debug Bones");
        ImGui::SameLine(100.0f);
        if (ImGui::Toggle("##BonesToggle", &showBones, config)) {
            context.debugRenderer->SetShowBones(showBones);
        }
    }

    ImGui::End();
}

void EditorUI::RenderHierarchy(const EditorContext& context) {
    if (!showHierarchy_) return;

    ImGui::Begin("Hierarchy", &showHierarchy_);

    if (context.gameObjects) {
        for (size_t i = 0; i < context.gameObjects->size(); ++i) {
            GameObject* obj = (*context.gameObjects)[i].get();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selectedObject_ == obj) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            // ユニークIDを生成（同じ名前のオブジェクトがあっても区別できるように）
            ImGui::PushID(static_cast<int>(i));
            ImGui::TreeNodeEx(obj->GetName().c_str(), flags);

            // クリックで選択
            if (ImGui::IsItemClicked()) {
                selectedObject_ = obj;
                // 選択したオブジェクトにカメラをフォーカス（ワールド行列から位置を取得）
                Matrix4x4 worldMatrix = obj->GetTransform().GetWorldMatrix();
                float m[16];
                worldMatrix.ToFloatArray(m);
                Vector3 targetPos(m[12], m[13], m[14]);
                editorCamera_.FocusOn(targetPos, 5.0f);
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

void EditorUI::RenderStats(const EditorContext& context) {
    if (!showStats_) return;

    ImGui::Begin("Stats", &showStats_);

    // Performance section
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.72f, 0.89f, 1.0f)); // Light blue header
    ImGui::Text("Performance");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // FPS display with color coding (update every 0.5 seconds)
    static float displayedFPS = 0.0f;
    static float displayedFrameTime = 0.0f;
    static float displayUpdateTimer = 0.0f;

    displayUpdateTimer += ImGui::GetIO().DeltaTime;
    if (displayUpdateTimer >= 0.5f) {
        displayedFPS = context.fps;
        displayedFrameTime = context.frameTime;
        displayUpdateTimer = 0.0f;
    }

    ImVec4 fpsColor = displayedFPS >= 60.0f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :  // Green if 60+ FPS
                      displayedFPS >= 30.0f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) :  // Yellow if 30-60 FPS
                                              ImVec4(1.0f, 0.0f, 0.0f, 1.0f);   // Red if < 30 FPS

    ImGui::Text("FPS:");
    ImGui::SameLine(120.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
    ImGui::Text("%.1f", displayedFPS);
    ImGui::PopStyleColor();

    ImGui::Text("Frame Time:");
    ImGui::SameLine(120.0f);
    ImGui::Text("%.3f ms", displayedFrameTime);

    // FPS Graph (update every 0.5 seconds)
    static float fpsHistory[90] = {};
    static int fpsOffset = 0;
    static float updateTimer = 0.0f;

    updateTimer += ImGui::GetIO().DeltaTime;
    if (updateTimer >= 0.5f) {
        fpsHistory[fpsOffset] = context.fps;
        fpsOffset = (fpsOffset + 1) % 90;
        updateTimer = 0.0f;
    }

    ImGui::Spacing();
    ImGui::PlotLines("##FPSGraph", fpsHistory, 90, fpsOffset, nullptr, 0.0f, 120.0f, ImVec2(0, 60));

    ImGui::Spacing();
    ImGui::Separator();

    // Scene statistics
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.72f, 0.89f, 1.0f)); // Light blue header
    ImGui::Text("Scene");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (context.gameObjects) {
        ImGui::Text("Objects:");
        ImGui::SameLine(120.0f);
        ImGui::Text("%zu", context.gameObjects->size());
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Camera information
    if (context.camera) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.72f, 0.89f, 1.0f)); // Light blue header
        ImGui::Text("Camera");
        ImGui::PopStyleColor();
        ImGui::Separator();

        auto pos = context.camera->GetPosition();
        ImGui::Text("Position:");
        ImGui::Indent(20.0f);
        ImGui::Text("X: %.2f", pos.GetX());
        ImGui::Text("Y: %.2f", pos.GetY());
        ImGui::Text("Z: %.2f", pos.GetZ());
        ImGui::Unindent(20.0f);
    }

    ImGui::End();
}

void EditorUI::RenderConsole() {
    if (!showConsole_) return;

    ImGui::Begin("Console", &showConsole_);

    if (ImGui::Button("Clear")) {
        consoleMessages_.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Test Log")) {
        consoleMessages_.push_back("[Info] Test log message");
    }

    ImGui::Separator();
    ImGui::BeginChild("ConsoleScrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& msg : consoleMessages_) {
        ImGui::TextUnformatted(msg.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

void EditorUI::RenderProject(const EditorContext& context) {
    if (!showProject_) return;

    ImGui::Begin("Project", &showProject_);

    ImGui::Text("Assets");
    ImGui::Separator();

    if (ImGui::TreeNode("Models")) {
        if (context.loadedModels.empty()) {
            ImGui::TextDisabled("(none)");
        } else {
            for (const auto& model : context.loadedModels) {
                ImGui::Selectable(model.c_str());
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Textures")) {
        if (context.loadedTextures.empty()) {
            ImGui::TextDisabled("(none)");
        } else {
            for (const auto& texture : context.loadedTextures) {
                ImGui::Selectable(texture.c_str());
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Scenes")) {
        if (context.currentSceneName.empty()) {
            ImGui::TextDisabled("(none)");
        } else {
            ImGui::Selectable(context.currentSceneName.c_str());
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

void EditorUI::RenderProfiler() {
    if (!showProfiler_) return;

    ImGui::Begin("Profiler", &showProfiler_);

    ImGui::Text("Performance Profiler");
    ImGui::Separator();

    static float values[90] = {};
    static int values_offset = 0;
    values[values_offset] = ImGui::GetIO().Framerate;
    values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);

    ImGui::PlotLines("FPS", values, IM_ARRAYSIZE(values), values_offset, nullptr, 0.0f, 120.0f, ImVec2(0, 80));

    ImGui::Separator();
    ImGui::Text("Draw Calls: N/A");
    ImGui::Text("Vertices: N/A");
    ImGui::Text("Triangles: N/A");

    ImGui::End();
}

void EditorUI::ProcessHotkeys() {
    ImGuiIO& io = ImGui::GetIO();
    
    // テキスト入力中はホットキーを無効化
    if (io.WantTextInput) return;

    // F5: Play/Pause切り替え
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false) && !io.KeyShift) {
        if (editorMode_ == EditorMode::Edit) {
            Play();
        } else if (editorMode_ == EditorMode::Play) {
            Pause();
        } else if (editorMode_ == EditorMode::Pause) {
            Play();
        }
    }

    // Escape: Stop（再生中/一時停止中）
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (editorMode_ != EditorMode::Edit) {
            Stop();
        }
    }

    // F1: Scene View表示トグル
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        showSceneView_ = !showSceneView_;
    }

    // F2: Game View表示トグル
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
        showGameView_ = !showGameView_;
    }

    // Q: 移動ギズモ
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false) && !io.KeyCtrl) {
        gizmoSystem_.SetOperation(GizmoOperation::Translate);
        consoleMessages_.push_back("[Editor] Gizmo: Translate");
    }

    // E: 回転ギズモ
    if (ImGui::IsKeyPressed(ImGuiKey_E, false) && !io.KeyCtrl) {
        gizmoSystem_.SetOperation(GizmoOperation::Rotate);
        consoleMessages_.push_back("[Editor] Gizmo: Rotate");
    }

    // R: スケールギズモ（Ctrl+Rはレイアウトリセット用なので除外）
    if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !io.KeyCtrl && !io.KeyShift) {
        gizmoSystem_.SetOperation(GizmoOperation::Scale);
        consoleMessages_.push_back("[Editor] Gizmo: Scale");
    }

    // G: Local/World切り替え
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) {
        if (gizmoSystem_.GetMode() == GizmoMode::World) {
            gizmoSystem_.SetMode(GizmoMode::Local);
            consoleMessages_.push_back("[Editor] Gizmo Mode: Local");
        } else {
            gizmoSystem_.SetMode(GizmoMode::World);
            consoleMessages_.push_back("[Editor] Gizmo Mode: World");
        }
    }

    // F10: Step（一時停止中のみ）
    if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) {
        if (editorMode_ == EditorMode::Pause) {
            Step();
        }
    }

    // Ctrl+Shift+R: レイアウトリセット
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        dockingLayoutInitialized_ = false;
        consoleMessages_.push_back("[Editor] Layout reset");
    }

    // Shift+F5: 停止（VSスタイル）
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (editorMode_ != EditorMode::Edit) {
            Stop();
        }
    }

    // Ctrl+Z: Undo（ギズモ操作を元に戻す）
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        PerformUndo();
    }

    // Ctrl+S: シーン保存
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        SaveScene("assets/scenes/default_scene.json");
    }
}

// Undo履歴に追加
void EditorUI::PushUndoSnapshot(const TransformSnapshot& snapshot) {
    undoStack_.push(snapshot);
    consoleMessages_.push_back("[Editor] Transform change recorded");
}

// Undo実行
void EditorUI::PerformUndo() {
    if (undoStack_.empty()) {
        consoleMessages_.push_back("[Editor] Nothing to undo");
        return;
    }

    TransformSnapshot snapshot = undoStack_.top();
    undoStack_.pop();

    if (snapshot.targetObject) {
        auto& transform = snapshot.targetObject->GetTransform();
        transform.SetLocalPosition(snapshot.position);
        transform.SetLocalRotation(snapshot.rotation);
        transform.SetLocalScale(snapshot.scale);
        consoleMessages_.push_back("[Editor] Undo performed");
    } else {
        consoleMessages_.push_back("[Editor] Undo failed: object no longer exists");
    }
}

// シーン保存
void EditorUI::SaveScene(const std::string& filepath) {
    if (!gameObjects_) {
        consoleMessages_.push_back("[Editor] Error: No game objects to save");
        return;
    }

    if (SceneSerializer::SaveScene(*gameObjects_, filepath)) {
        consoleMessages_.push_back("[Editor] Scene saved: " + filepath);
    } else {
        consoleMessages_.push_back("[Editor] Failed to save scene: " + filepath);
    }
}

// シーンロード
void EditorUI::LoadScene(const std::string& filepath) {
    if (!gameObjects_) {
        consoleMessages_.push_back("[Editor] Error: No game objects container");
        return;
    }

    if (SceneSerializer::LoadScene(filepath, *gameObjects_)) {
        consoleMessages_.push_back("[Editor] Scene loaded: " + filepath);
        // ロード後、最初のオブジェクトを選択
        if (!gameObjects_->empty()) {
            selectedObject_ = (*gameObjects_)[0].get();
        }
    } else {
        consoleMessages_.push_back("[Editor] Failed to load scene: " + filepath);
    }
}

} // namespace UnoEngine
