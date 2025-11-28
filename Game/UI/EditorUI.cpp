#include "EditorUI.h"
#include "../../Engine/Graphics/GraphicsDevice.h"
#include "../../Engine/Rendering/DebugRenderer.h"
#include "../../Engine/Animation/AnimationSystem.h"
#include "../../Engine/Scene/SceneSerializer.h"
#include "../../Engine/Rendering/SkinnedMeshRenderer.h"
#include "../../Engine/Animation/AnimatorComponent.h"
#include "../../Engine/Resource/ResourceManager.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "../../Engine/UI/imgui_toggle.h"
#include "../../Engine/UI/imgui_toggle_presets.h"
#include "ImGuizmo.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>

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
			// オブジェクト選択中はWASD移動を無効化
			editorCamera_.SetMovementEnabled(selectedObject_ == nullptr);
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
		}
		else if (editorMode_ == EditorMode::Pause) {
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
				}
				else {
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
			}
			else if (isPaused) {
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
		}
		else {
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

		// ヘッダーバー
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
		ImGui::Text("🌳 Scene Objects");
		ImGui::PopStyleColor();
		ImGui::Separator();

		// 選択解除ボタン
		if (selectedObject_ && ImGui::SmallButton("Clear Selection")) {
			selectedObject_ = nullptr;
		}
		ImGui::SameLine();
		if (context.gameObjects) {
			ImGui::TextDisabled("(%zu objects)", context.gameObjects->size());
		}
		ImGui::Separator();

		// オブジェクトリスト
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
				
				// コンポーネントに応じたアイコン
				const char* icon = "📦";
				if (obj->GetComponent<SkinnedMeshRenderer>()) icon = "🎭";
				else if (obj->GetComponent<DirectionalLightComponent>()) icon = "💡";
				else if (obj->GetName() == "Player") icon = "🎮";
				else if (obj->GetName().find("Camera") != std::string::npos) icon = "📷";
				
				// アイコンとオブジェクト名を表示
				ImGui::Text("%s", icon);
				ImGui::SameLine();
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
				
				// 右クリックメニュー
				if (ImGui::BeginPopupContextItem()) {
					if (ImGui::MenuItem("Focus")) {
						Matrix4x4 worldMatrix = obj->GetTransform().GetWorldMatrix();
						float m[16];
						worldMatrix.ToFloatArray(m);
						Vector3 targetPos(m[12], m[13], m[14]);
						editorCamera_.FocusOn(targetPos, 5.0f);
					}
					if (ImGui::MenuItem("Delete", "DEL")) {
						// 削除対象としてマーク（ループ中に直接削除すると問題があるため）
						if (gameObjects_) {
							for (auto it = gameObjects_->begin(); it != gameObjects_->end(); ++it) {
								if (it->get() == obj) {
									consoleMessages_.push_back("[Editor] Deleted object: " + obj->GetName());
									gameObjects_->erase(it);
									if (selectedObject_ == obj) {
										selectedObject_ = nullptr;
									}
									break;
								}
							}
						}
					}
					ImGui::EndPopup();
				}
				
				ImGui::PopID();
			}
		}
		else {
			ImGui::TextDisabled("(no objects)");
		}

		// DELキーで選択中のオブジェクトを削除
		if (selectedObject_ && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
			if (gameObjects_) {
				for (auto it = gameObjects_->begin(); it != gameObjects_->end(); ++it) {
					if (it->get() == selectedObject_) {
						consoleMessages_.push_back("[Editor] Deleted object (DEL): " + selectedObject_->GetName());
						gameObjects_->erase(it);
						selectedObject_ = nullptr;
						break;
					}
				}
			}
		}

		// ウィンドウ全体をドロップターゲットに（背景エリア）
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImGui::SetCursorPos(ImVec2(0, ImGui::GetCursorPosY()));
		ImGui::InvisibleButton("##HierarchyDropZone", ImVec2(windowSize.x, 100.0f));
		
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_INDEX")) {
				size_t modelIndex = *static_cast<const size_t*>(payload->Data);
				HandleModelDragDropByIndex(modelIndex);
			}
			ImGui::EndDragDropTarget();
		}

		// ドロップゾーンのヒント表示
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Drop models here to add to scene");
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

		// Modelsフォルダをスキャン
		if (ImGui::TreeNode("Models")) {
			// リフレッシュボタン
			if (ImGui::SmallButton("Refresh")) {
				RefreshModelPaths();
				consoleMessages_.push_back("[Editor] Model list refreshed");
			}
			ImGui::Separator();

			// 初回またはリフレッシュ時にスキャン
			if (cachedModelPaths_.empty()) {
				RefreshModelPaths();
			}

			// モデルリストを表示
			for (size_t i = 0; i < cachedModelPaths_.size(); ++i) {
				const auto& modelPath = cachedModelPaths_[i];
				std::filesystem::path p(modelPath);
				std::string filename = p.filename().string();

				ImGui::PushID(static_cast<int>(i));
				
				// アイコン表示（ファイル拡張子に応じて）
			std::string ext = p.extension().string();

			// OBJファイルはスキンメッシュ非対応なのでスキップ
			if (ext == ".obj") {
				ImGui::PopID();
				continue;
			}

			const char* icon = "📦"; // デフォルトアイコン
			if (ext == ".gltf" || ext == ".glb") icon = "🎨";
			else if (ext == ".fbx") icon = "🔷";
				
				ImGui::Text("%s", icon);
				ImGui::SameLine();
				
				bool selected = false;
				if (ImGui::Selectable(filename.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
					// ダブルクリックでシーンに追加
					if (ImGui::IsMouseDoubleClicked(0)) {
						HandleModelDragDropByIndex(i);
					}
				}

				// ドラッグソース設定
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
					ImGui::SetDragDropPayload("MODEL_INDEX", &i, sizeof(size_t));
					ImGui::Text("🎯 Drag: %s", filename.c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::PopID();
			}
			
			if (cachedModelPaths_.empty()) {
				ImGui::TextDisabled("(no models found)");
			}
			
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Textures")) {
			if (context.loadedTextures.empty()) {
				ImGui::TextDisabled("(none)");
			}
			else {
				for (const auto& texture : context.loadedTextures) {
					ImGui::Selectable(texture.c_str());
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Scenes")) {
			if (context.currentSceneName.empty()) {
				ImGui::TextDisabled("(none)");
			}
			else {
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
			}
			else if (editorMode_ == EditorMode::Play) {
				Pause();
			}
			else if (editorMode_ == EditorMode::Pause) {
				Play();
			}
		}

		// Escape: Stop（再生中/一時停止中）または選択解除（編集モード）
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
			if (editorMode_ != EditorMode::Edit) {
				Stop();
			} else {
				// 編集モードでは選択をクリア
				selectedObject_ = nullptr;
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
			}
			else {
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
		}
		else {
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
		}
		else {
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
		}
		else {
			consoleMessages_.push_back("[Editor] Failed to load scene: " + filepath);
		}
	}

	// モデルD&D処理（パスから）
	void EditorUI::HandleModelDragDrop(const std::string& modelPath) {
		if (!gameObjects_ || !resourceManager_) {
			consoleMessages_.push_back("[Editor] Error: Cannot create object - missing dependencies");
			return;
		}

		// 遅延ロードキューに追加
		pendingModelLoads_.push_back(modelPath);
		consoleMessages_.push_back("[Editor] Model queued for loading: " + modelPath);
	}

	// モデルD&D処理（インデックスから）
	void EditorUI::HandleModelDragDropByIndex(size_t modelIndex) {
		if (modelIndex < cachedModelPaths_.size()) {
			HandleModelDragDrop(cachedModelPaths_[modelIndex]);
		}
		else {
			consoleMessages_.push_back("[Editor] Error: Invalid model index");
		}
	}

	// モデルパスをリフレッシュ
	void EditorUI::RefreshModelPaths() {
		cachedModelPaths_.clear();

		std::string modelsPath = "assets/model";
		if (std::filesystem::exists(modelsPath) && std::filesystem::is_directory(modelsPath)) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsPath)) {
				if (entry.is_regular_file()) {
					std::string ext = entry.path().extension().string();
					if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
						std::string relativePath = entry.path().string();
						std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
						cachedModelPaths_.push_back(relativePath);
					}
				}
			}
		}
	}

	// 遅延ロード処理
	void EditorUI::ProcessPendingLoads() {
		if (pendingModelLoads_.empty()) return;
		if (!gameObjects_ || !resourceManager_) return;

		// 各モデルを個別に処理（複数モデルを1つのアップロードコンテキストで処理すると描画バグが発生するため）
	for (const auto& modelPath : pendingModelLoads_) {
			consoleMessages_.push_back("[Editor] Loading model: " + modelPath);

			// モデル名を取得（拡張子なし）
		std::filesystem::path path(modelPath);
		std::string modelName = path.stem().string();

		// このモデル専用のアップロードコンテキスト
		resourceManager_->BeginUpload();

		// モデルをロード
		auto* modelData = resourceManager_->LoadSkinnedModel(modelPath);

		// アップロード完了
		resourceManager_->EndUpload();

			if (!modelData) {
				consoleMessages_.push_back("[Editor] ERROR: Failed to load model: " + modelPath);
				continue;
			}

			consoleMessages_.push_back("[Editor] Model loaded successfully");

			// GameObjectを生成
			auto newObject = std::make_unique<GameObject>(modelName);

			// AnimatorComponentを先に追加（SkinnedMeshRenderer::Awake()でリンクできるように）
			auto* animator = newObject->AddComponent<AnimatorComponent>();
			if (modelData->skeleton) {
				animator->Initialize(modelData->skeleton, modelData->animations);
				if (!modelData->animations.empty()) {
					std::string animName = modelData->animations[0]->GetName();
					animator->Play(animName, true);
					consoleMessages_.push_back("[Editor] Playing animation: " + animName);
				}
			}

			// SkinnedMeshRendererを追加（AnimatorComponentが既に存在するのでAwake()でリンクされる）
			auto* renderer = newObject->AddComponent<SkinnedMeshRenderer>();
			renderer->SetModel(modelPath);  // まずパスを設定
			renderer->SetModel(modelData);   // 次に実際のモデルデータを設定

			// 選択状態にする
			selectedObject_ = newObject.get();

			// GameObjectsリストに追加
			gameObjects_->push_back(std::move(newObject));

			// 重要: コンポーネントのStart()を呼んで初期化
			// （再起動時はScene::ProcessPendingStarts()で呼ばれるが、D&D時は手動で呼ぶ必要がある）
			if (scene_) {
				scene_->StartGameObject(selectedObject_);
			}

			consoleMessages_.push_back("[Editor] Created object: " + modelName);
	}

	// キューをクリア
		pendingModelLoads_.clear();
	}

} // namespace UnoEngine
