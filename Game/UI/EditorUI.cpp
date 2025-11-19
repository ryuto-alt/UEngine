#include "EditorUI.h"
#include "../../Engine/Graphics/GraphicsDevice.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace UnoEngine {

void EditorUI::Initialize(GraphicsDevice* graphics) {
    // RenderTexture setup (SRVインデックス 3と4を使用) - 16:9 aspect ratio
    gameViewTexture_.Create(graphics, 1280, 720, 3);
    sceneViewTexture_.Create(graphics, 1280, 720, 4);

    // Console初期ログ
    consoleMessages_.push_back("[System] UnoEngine Editor Initialized");
    consoleMessages_.push_back("[Info] Press ~ to toggle console");
}

void EditorUI::Render(const EditorContext& context) {
    RenderDockSpace();
    RenderGameView();
    RenderSceneView();
    RenderInspector(context);
    RenderHierarchy(context);
    RenderStats(context);
    RenderConsole();
    RenderProject();
    RenderProfiler();
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
            ImGui::MenuItem("Game View", nullptr, &showGameView_);
            ImGui::MenuItem("Debug View", nullptr, &showSceneView_);

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
        ImGuiID dock_game_view, dock_debug_view;
        ImGuiID dock_project, dock_console;

        // 上(55%) | 下(45%)
        dock_top = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Up, 0.55f, nullptr, &dock_bottom);

        // 上部を左(20%) | 右(80%)に分割
        dock_left = ImGui::DockBuilderSplitNode(dock_top, ImGuiDir_Left, 0.20f, nullptr, &dock_right);

        // 右上部を左右に分割（Game View 50% | Debug View 50%）
        dock_game_view = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Left, 0.5f, nullptr, &dock_debug_view);

        // 下部を左右に分割（Project 20% | Console 80%）
        dock_project = ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Left, 0.20f, nullptr, &dock_console);

        // パネルをドックに配置
        // 左上: Hierarchy, Inspector, Stats, Profiler（タブ）
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_left);
        ImGui::DockBuilderDockWindow("Stats", dock_left);
        ImGui::DockBuilderDockWindow("Profiler", dock_left);

        // 右上: Game View（左）、Debug View（右）
        ImGui::DockBuilderDockWindow("Game View", dock_game_view);
        ImGui::DockBuilderDockWindow("Debug View", dock_debug_view);

        // 下部: Project（左）、Console（右）
        ImGui::DockBuilderDockWindow("Project", dock_project);
        ImGui::DockBuilderDockWindow("Console", dock_console);

        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();
}

void EditorUI::RenderGameView() {
    if (!showGameView_) return;

    ImGui::Begin("Game View", &showGameView_);
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    if (availableSize.x > 0 && availableSize.y > 0) {
        // 16:9のアスペクト比を維持した最適なサイズを計算
        const float aspectRatio = 16.0f / 9.0f;
        ImVec2 imageSize;

        // 幅基準で高さを計算
        imageSize.x = availableSize.x;
        imageSize.y = availableSize.x / aspectRatio;

        // 高さが利用可能スペースを超える場合は、高さ基準で計算
        if (imageSize.y > availableSize.y) {
            imageSize.y = availableSize.y;
            imageSize.x = availableSize.y * aspectRatio;
        }

        // 中央配置のための余白を計算
        ImVec2 cursorPos = ImGui::GetCursorPos();
        cursorPos.x += (availableSize.x - imageSize.x) * 0.5f;
        cursorPos.y += (availableSize.y - imageSize.y) * 0.5f;
        ImGui::SetCursorPos(cursorPos);

        // 次のフレームで適用するサイズを記録（16:9を維持）
        desiredGameViewWidth_ = static_cast<uint32>(imageSize.x);
        desiredGameViewHeight_ = static_cast<uint32>(imageSize.y);

        // テクスチャを表示
        ImGui::Image((ImTextureID)gameViewTexture_.GetSRVHandle().ptr, imageSize);
    }

    ImGui::End();
}

void EditorUI::RenderSceneView() {
    if (!showSceneView_) return;

    ImGui::Begin("Debug View", &showSceneView_);
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    if (availableSize.x > 0 && availableSize.y > 0) {
        // 16:9のアスペクト比を維持した最適なサイズを計算
        const float aspectRatio = 16.0f / 9.0f;
        ImVec2 imageSize;

        // 幅基準で高さを計算
        imageSize.x = availableSize.x;
        imageSize.y = availableSize.x / aspectRatio;

        // 高さが利用可能スペースを超える場合は、高さ基準で計算
        if (imageSize.y > availableSize.y) {
            imageSize.y = availableSize.y;
            imageSize.x = availableSize.y * aspectRatio;
        }

        // 中央配置のための余白を計算
        ImVec2 cursorPos = ImGui::GetCursorPos();
        cursorPos.x += (availableSize.x - imageSize.x) * 0.5f;
        cursorPos.y += (availableSize.y - imageSize.y) * 0.5f;
        ImGui::SetCursorPos(cursorPos);

        // 次のフレームで適用するサイズを記録（16:9を維持）
        desiredSceneViewWidth_ = static_cast<uint32>(imageSize.x);
        desiredSceneViewHeight_ = static_cast<uint32>(imageSize.y);

        // テクスチャを表示
        ImGui::Image((ImTextureID)sceneViewTexture_.GetSRVHandle().ptr, imageSize);
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

    ImGui::End();
}

void EditorUI::RenderHierarchy(const EditorContext& context) {
    if (!showHierarchy_) return;

    ImGui::Begin("Hierarchy", &showHierarchy_);

    if (context.gameObjects) {
        for (const auto& obj : *context.gameObjects) {
            if (ImGui::Selectable(obj->GetName().c_str(), selectedObject_ == obj.get())) {
                selectedObject_ = obj.get();
            }
        }
    }

    ImGui::End();
}

void EditorUI::RenderStats(const EditorContext& context) {
    if (!showStats_) return;

    ImGui::Begin("Stats", &showStats_);

    if (context.camera) {
        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
            context.camera->GetPosition().GetX(),
            context.camera->GetPosition().GetY(),
            context.camera->GetPosition().GetZ());
    }

    ImGui::Separator();
    if (context.gameObjects) {
        ImGui::Text("Objects: %zu", context.gameObjects->size());
    }
    ImGui::Text("FPS: %.1f", context.fps);
    ImGui::Text("Frame Time: %.3f ms", context.frameTime);

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

void EditorUI::RenderProject() {
    if (!showProject_) return;

    ImGui::Begin("Project", &showProject_);

    ImGui::Text("Assets");
    ImGui::Separator();

    if (ImGui::TreeNode("Models")) {
        ImGui::Selectable("testmodel.obj");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Textures")) {
        ImGui::Selectable("ruru6.png");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Scenes")) {
        ImGui::Selectable("GameScene");
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

} // namespace UnoEngine
