#include "GameScene.h"
#include "../Components/Player.h"
#include "../GameApplication.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Graphics/MeshRenderer.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Math/Math.h"
#include "../../Engine/UI/ImGuiManager.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace UnoEngine {

void GameScene::OnLoad() {
    SetActiveCamera(nullptr);

    // Camera setup
    auto camera = MakeUnique<Camera>();
    camera->SetPosition(Vector3(0.0f, 5.0f, 10.0f));
    camera->SetRotation(Quaternion::LookRotation(Vector3(0.0f, -0.5f, -1.0f).Normalize(), Vector3::UnitY()));
    SetActiveCamera(camera.release());

    // Player setup
    player_ = CreateGameObject("Player");
    auto* app = static_cast<GameApplication*>(GetApplication());
    auto* mesh = app->LoadMesh("resources/model/testmodel/testmodel.obj");
    player_->AddComponent<MeshRenderer>(mesh, const_cast<Material*>(mesh->GetMaterial()));
    player_->AddComponent<Player>();

    // Light setup
    auto* light = CreateGameObject("DirectionalLight");
    auto* lightComp = light->AddComponent<DirectionalLightComponent>();
    lightComp->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
    lightComp->SetColor(Vector3(1.0f, 1.0f, 1.0f));
    lightComp->SetIntensity(1.0f);
    lightComp->UseTransformDirection(false);

    // RenderTexture setup (SRVインデックス 3と4を使用) - 16:9 aspect ratio
    gameViewTexture_.Create(app->GetGraphicsDevice(), 1280, 720, 3);
    sceneViewTexture_.Create(app->GetGraphicsDevice(), 1280, 720, 4);

    // Console初期ログ
    consoleMessages_.push_back("[System] UnoEngine Editor Initialized");
    consoleMessages_.push_back("[Scene] GameScene loaded successfully");
    consoleMessages_.push_back("[Info] Press ~ to toggle console");
}

void GameScene::OnUpdate(float deltaTime) {
    // 前のフレームで記録したサイズでRenderTextureをリサイズ（描画前に実行）
    auto* app = static_cast<GameApplication*>(GetApplication());
    gameViewTexture_.Resize(app->GetGraphicsDevice(), desiredGameViewWidth_, desiredGameViewHeight_);
    sceneViewTexture_.Resize(app->GetGraphicsDevice(), desiredSceneViewWidth_, desiredSceneViewHeight_);

    // Rotate player
    if (player_) {
        auto& transform = player_->GetTransform();
        Quaternion currentRotation = transform.GetLocalRotation();
        Quaternion deltaRotation = Quaternion::RotationAxis(Vector3::UnitY(), Math::ToRadians(30.0f) * deltaTime);
        transform.SetLocalRotation(deltaRotation * currentRotation);
    }

    // Update camera input via player system
    Camera* camera = GetActiveCamera();
    if (camera && player_) {
        auto* playerComp = player_->GetComponent<Player>();
        auto* app = static_cast<GameApplication*>(GetApplication());
        app->GetPlayerSystem().Update(camera, playerComp, input_, deltaTime);
    }
}

void GameScene::OnImGui() {
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

    // 初回起動時にデフォルトレイアウトを構築
    if (!dockingLayoutInitialized_) {
        dockingLayoutInitialized_ = true;

        // レイアウトをリセット
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

        // ドックスペースを分割
        ImGuiID dock_left, dock_left_top, dock_left_bottom;
        ImGuiID dock_right, dock_right_top, dock_right_bottom;
        ImGuiID dock_game_view, dock_debug_view;

        // 左(20%) | 右(80%)
        dock_left = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 0.20f, nullptr, &dock_right);

        // 左側を上下に分割（上60% = Hierarchy/Inspector/Stats, 下40% = Project）
        dock_left_top = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, 0.60f, nullptr, &dock_left_bottom);

        // 右側を上下に分割（上75% = Viewports, 下25% = Console）
        dock_right_top = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.75f, nullptr, &dock_right_bottom);

        // 右上部を左右に分割（Game View 50% | Debug View 50%）
        dock_game_view = ImGui::DockBuilderSplitNode(dock_right_top, ImGuiDir_Left, 0.5f, nullptr, &dock_debug_view);

        // パネルをドックに配置
        // 左上: Hierarchy, Inspector, Stats（タブ）
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left_top);
        ImGui::DockBuilderDockWindow("Inspector", dock_left_top);
        ImGui::DockBuilderDockWindow("Stats", dock_left_top);
        ImGui::DockBuilderDockWindow("Profiler", dock_left_top);

        // 左下: Project
        ImGui::DockBuilderDockWindow("Project", dock_left_bottom);

        // 右上: Game View（左）、Debug View（右）
        ImGui::DockBuilderDockWindow("Game View", dock_game_view);
        ImGui::DockBuilderDockWindow("Debug View", dock_debug_view);

        // 右下: Console
        ImGui::DockBuilderDockWindow("Console", dock_right_bottom);

        ImGui::DockBuilderFinish(dockspaceID);
    }

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

    ImGui::End();

    // Game View (Runtime view)
    if (showGameView_) {
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

    // Debug View (Editor view with debug tools)
    if (showSceneView_) {
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

    // Inspector
    if (showInspector_) {
        ImGui::Begin("Inspector", &showInspector_);

        if (player_) {
            ImGui::Text("Selected: Player");
            ImGui::Separator();

            auto& transform = player_->GetTransform();
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

    // Hierarchy
    if (showHierarchy_) {
        ImGui::Begin("Hierarchy", &showHierarchy_);

        const auto& objects = GetGameObjects();
        for (const auto& obj : objects) {
            if (ImGui::Selectable(obj->GetName().c_str(), selectedObject_ == obj.get())) {
                selectedObject_ = obj.get();
            }
        }

        ImGui::End();
    }

    // Stats
    if (showStats_) {
        ImGui::Begin("Stats", &showStats_);

        Camera* camera = GetActiveCamera();
        if (camera) {
            ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                camera->GetPosition().GetX(),
                camera->GetPosition().GetY(),
                camera->GetPosition().GetZ());
        }

        ImGui::Separator();
        ImGui::Text("Objects: %zu", GetGameObjects().size());
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

        ImGui::End();
    }

    // Console
    if (showConsole_) {
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

    // Project (Assets)
    if (showProject_) {
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

    // Profiler
    if (showProfiler_) {
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
}

void GameScene::OnRender(RenderView& view) {
    Camera* camera = GetActiveCamera();
    if (!camera) return;
    
    view.camera = camera;
    view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
    view.viewName = "MainView";


}

} // namespace UnoEngine
