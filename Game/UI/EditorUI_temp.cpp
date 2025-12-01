#include "EditorUI.h"
#include "../../Engine/Graphics/GraphicsDevice.h"
#include "../../Engine/Rendering/DebugRenderer.h"
#include "../../Engine/Animation/AnimationSystem.h"
#include "../../Engine/Scene/SceneSerializer.h"
#include "../../Engine/Rendering/SkinnedMeshRenderer.h"
#include "../../Engine/Animation/AnimatorComponent.h"
#include "../../Engine/Resource/ResourceManager.h"
#include "../../Engine/Resource/SkinnedModelImporter.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Math/BoundingVolume.h"
#include "../../Engine/Audio/AudioSystem.h"
#include "../../Engine/Audio/AudioSource.h"
#include "../../Engine/Audio/AudioListener.h"
#include "../../Engine/Audio/AudioClip.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "../../Engine/UI/imgui_toggle.h"
#include "../../Engine/UI/imgui_toggle_presets.h"
#include "ImGuizmo.h"
#include <algorithm>
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>

namespace UnoEngine {

	void EditorUI::Initialize(GraphicsDevice* graphics) {
		// RenderTexture setup (SRV繧､繝ｳ繝・ャ繧ｯ繧ｹ 3縺ｨ4繧剃ｽｿ逕ｨ) - 16:9 aspect ratio
		gameViewTexture_.Create(graphics, 1280, 720, 3);
		sceneViewTexture_.Create(graphics, 1280, 720, 4);

		// 繧ｮ繧ｺ繝｢繧ｷ繧ｹ繝・Β蛻晄悄蛹・
		gizmoSystem_.Initialize();

		// Console蛻晄悄繝ｭ繧ｰ
		consoleMessages_.push_back("[System] UnoEngine Editor Initialized");
		consoleMessages_.push_back("[Info] Press ~ to toggle console");
		consoleMessages_.push_back("[Info] Q: Translate, E: Rotate, R: Scale");
	}

	void EditorUI::Render(const EditorContext& context) {
		// ImGuizmo繝輔Ξ繝ｼ繝髢句ｧ・
		ImGuizmo::BeginFrame();

		// 繧ｫ繝｡繝ｩ繧定ｨｭ螳夲ｼ域ｯ弱ヵ繝ｬ繝ｼ繝・・
		if (context.camera) {
			editorCamera_.SetCamera(context.camera);
		}

		// 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ繧ｷ繧ｹ繝・Β繧定ｨｭ螳・
		if (context.animationSystem) {
			animationSystem_ = context.animationSystem;
		}

		// 繝帙ャ繝医く繝ｼ蜃ｦ逅・
		ProcessHotkeys();

		RenderDockSpace();
		RenderSceneView();
		RenderGameView();
		RenderHierarchy(context);
		RenderInspector(context);
		RenderStats(context);
		RenderConsole();
		RenderProject(context);
		RenderProfiler();

		// 繧ｨ繝・ぅ繧ｿ繧ｫ繝｡繝ｩ縺ｮ譖ｴ譁ｰ・・dit/Pause繝｢繝ｼ繝峨・縺ｿ・・
		if (editorMode_ != EditorMode::Play) {
			float deltaTime = ImGui::GetIO().DeltaTime;
			editorCamera_.SetMovementEnabled(true);
			editorCamera_.Update(deltaTime);
		}

		// 繧ｹ繝・ャ繝励ヵ繝ｬ繝ｼ繝繧偵Μ繧ｻ繝・ヨ
		stepFrame_ = false;
	}

	void EditorUI::Play() {
		if (editorMode_ == EditorMode::Edit) {
			editorMode_ = EditorMode::Play;
			// 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蜀咲函髢句ｧ・
			if (animationSystem_) {
				animationSystem_->SetPlaying(true);
			}
			consoleMessages_.push_back("[Editor] Play mode started");
		}
		else if (editorMode_ == EditorMode::Pause) {
			editorMode_ = EditorMode::Play;
			// 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蜀埼幕
			if (animationSystem_) {
				animationSystem_->SetPlaying(true);
			}
			consoleMessages_.push_back("[Editor] Resumed");
		}
	}

	void EditorUI::Pause() {
		if (editorMode_ == EditorMode::Play) {
			editorMode_ = EditorMode::Pause;
			// 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ荳譎ょ●豁｢
			if (animationSystem_) {
				animationSystem_->SetPlaying(false);
			}
			consoleMessages_.push_back("[Editor] Paused");
		}
	}

	void EditorUI::Stop() {
		if (editorMode_ != EditorMode::Edit) {
			editorMode_ = EditorMode::Edit;
			// 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蛛懈ｭ｢
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

			// Play/Pause/Stop 繝懊ち繝ｳ繧偵Γ繝九Η繝ｼ繝舌・荳ｭ螟ｮ縺ｫ驟咲ｽｮ
			float menuBarWidth = ImGui::GetWindowWidth();
			float buttonWidth = 28.0f;
			float totalWidth = buttonWidth * 3 + 8.0f;
			float startX = (menuBarWidth - totalWidth) * 0.5f;

			ImGui::SetCursorPosX(startX);

			bool isPlaying = (editorMode_ == EditorMode::Play);
			bool isPaused = (editorMode_ == EditorMode::Pause);

			// Play/Pause繝懊ち繝ｳ
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

			// Stop繝懊ち繝ｳ
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

			// Step繝懊ち繝ｳ
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

			// 繝｢繝ｼ繝芽｡ｨ遉ｺ
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

		// 蛻晏屓襍ｷ蜍墓凾縺ｫ繝・ヵ繧ｩ繝ｫ繝医Ξ繧､繧｢繧ｦ繝医ｒ讒狗ｯ・
		if (!dockingLayoutInitialized_) {
			dockingLayoutInitialized_ = true;

			// 繝ｬ繧､繧｢繧ｦ繝医ｒ繝ｪ繧ｻ繝・ヨ
			ImGui::DockBuilderRemoveNode(dockspaceID);
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

			// 繝峨ャ繧ｯ繧ｹ繝壹・繧ｹ繧貞・蜑ｲ
			ImGuiID dock_top, dock_bottom;
			ImGuiID dock_left, dock_right;
			ImGuiID dock_scene, dock_game;
			ImGuiID dock_project, dock_console;

			// 荳・65%) | 荳・35%)
			dock_top = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Up, 0.65f, nullptr, &dock_bottom);

			// 荳企Κ繧貞ｷｦ(20%) | 蜿ｳ(80%)縺ｫ蛻・牡
			dock_left = ImGui::DockBuilderSplitNode(dock_top, ImGuiDir_Left, 0.20f, nullptr, &dock_right);

			// 蜿ｳ荳企Κ繧貞ｷｦ蜿ｳ縺ｫ蛻・牡・・cene 50% | Game 50%・・
			dock_scene = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Left, 0.5f, nullptr, &dock_game);

			// 荳矩Κ繧貞ｷｦ蜿ｳ縺ｫ蛻・牡・・roject 20% | Console 80%・・
			dock_project = ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Left, 0.20f, nullptr, &dock_console);

			// 繝代ロ繝ｫ繧偵ラ繝・け縺ｫ驟咲ｽｮ
			// 蟾ｦ荳・ Hierarchy, Inspector, Stats, Profiler・医ち繝厄ｼ・
			// 豕ｨ諢・ 譛蠕後↓Dock縺励◆繧ｦ繧｣繝ｳ繝峨え縺後い繧ｯ繝・ぅ繝悶ち繝悶↓縺ｪ繧・
			ImGui::DockBuilderDockWindow("Inspector", dock_left);
			ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
			ImGui::DockBuilderDockWindow("Stats", dock_left);
			ImGui::DockBuilderDockWindow("Profiler", dock_left);

			// 荳ｭ螟ｮ: Scene・亥ｷｦ・峨；ame・亥承・・
			ImGui::DockBuilderDockWindow("Scene", dock_scene);
			ImGui::DockBuilderDockWindow("Game", dock_game);

			// 荳矩Κ: Project・亥ｷｦ・峨，onsole・亥承・・
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

			// Scene View逕ｻ蜒上・繝帙ヰ繝ｼ迥ｶ諷九〒繧ｫ繝｡繝ｩ謫堺ｽ懊ｒ譛牙柑蛹・
			bool sceneHovered = ImGui::IsItemHovered();
			editorCamera_.SetViewportHovered(sceneHovered);
			editorCamera_.SetViewportFocused(ImGui::IsWindowFocused());

			// 逕ｻ蜒上・螳滄圀縺ｮ繧ｹ繧ｯ繝ｪ繝ｼ繝ｳ菴咲ｽｮ繧定ｨ育ｮ暦ｼ医ぐ繧ｺ繝｢逕ｨ・・
			// GetItemRectMin()縺ｧ逶ｴ蜑阪↓謠冗判縺励◆Image 縺ｮ豁｣遒ｺ縺ｪ繧ｹ繧ｯ繝ｪ繝ｼ繝ｳ蠎ｧ讓吶ｒ蜿門ｾ・
			ImVec2 imageMin = ImGui::GetItemRectMin();
			ImVec2 imageMax = ImGui::GetItemRectMax();
			sceneViewPosX_ = imageMin.x;
			sceneViewPosY_ = imageMin.y;
			sceneViewSizeX_ = imageMax.x - imageMin.x;
			sceneViewSizeY_ = imageMax.y - imageMin.y;

			// 繧ｨ繝・ぅ繧ｿ繧ｫ繝｡繝ｩ縺ｫ繝薙Η繝ｼ繝昴・繝育洸蠖｢繧定ｨｭ螳夲ｼ医・繧ｦ繧ｹ繧ｯ繝ｪ繝・・逕ｨ・・
			editorCamera_.SetViewportRect(sceneViewPosX_, sceneViewPosY_, sceneViewSizeX_, sceneViewSizeY_);

			// 繧ｮ繧ｺ繝｢謠冗判・・dit/Pause繝｢繝ｼ繝峨°縺､繧ｪ繝悶ず繧ｧ繧ｯ繝医′驕ｸ謚槭＆繧後※縺・ｋ蝣ｴ蜷茨ｼ・
			if (editorMode_ != EditorMode::Play && selectedObject_ && editorCamera_.GetCamera()) {
				// 繧ｮ繧ｺ繝｢謫堺ｽ憺幕蟋区凾縺ｫ繧ｹ繝翫ャ繝励す繝ｧ繝・ヨ繧剃ｿ晏ｭ・
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

				// 繧ｮ繧ｺ繝｢謫堺ｽ懃ｵゆｺ・凾縺ｫ螻･豁ｴ縺ｫ霑ｽ蜉
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

		// 繝倥ャ繝繝ｼ繝舌・
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
		ImGui::Text("元 Scene Objects");
		ImGui::PopStyleColor();
		ImGui::Separator();

		// 驕ｸ謚櫁ｧ｣髯､繝懊ち繝ｳ
		if (selectedObject_ && ImGui::SmallButton("Clear Selection")) {
			selectedObject_ = nullptr;
		}
		ImGui::SameLine();
		if (context.gameObjects) {
			ImGui::TextDisabled("(%zu objects)", context.gameObjects->size());
		}
		ImGui::Separator();

		// 繧ｪ繝悶ず繧ｧ繧ｯ繝医Μ繧ｹ繝・
		if (context.gameObjects) {
			for (size_t i = 0; i < context.gameObjects->size(); ++i) {
				GameObject* obj = (*context.gameObjects)[i].get();
				bool isExpanded = expandedObjects_.count(obj) > 0;
				bool isRenaming = (renamingObject_ == obj);

				// 繝ｦ繝九・繧ｯID繧堤函謌・
				ImGui::PushID(static_cast<int>(i));

				// 繧ｳ繝ｳ繝昴・繝阪Φ繝医↓蠢懊§縺溘い繧､繧ｳ繝ｳ
				const char* icon = "逃";
				if (obj->GetComponent<SkinnedMeshRenderer>()) icon = "鹿";
				else if (obj->GetComponent<DirectionalLightComponent>()) icon = "庁";
				else if (obj->GetName() == "Player") icon = "式";
				else if (obj->GetName().find("Camera") != std::string::npos) icon = "胴";

				// 螻暮幕遏｢蜊ｰ・亥ｰ上＆縺・ｸ芽ｧ貞ｽ｢・・
				bool hasTransformInfo = true;  // 蜈ｨ繧ｪ繝悶ず繧ｧ繧ｯ繝医↓Transform諠・ｱ縺ゅｊ
				if (hasTransformInfo) {
					// 蟆上＆縺・・繧ｿ繝ｳ縺ｧ荳芽ｧ貞ｽ｢繧定｡ｨ遉ｺ
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
					const char* arrowText = isExpanded ? "v" : ">";
					if (ImGui::SmallButton(arrowText)) {
						if (isExpanded) {
							expandedObjects_.erase(obj);
						} else {
							expandedObjects_.insert(obj);
						}
					}
					ImGui::PopStyleVar();
					ImGui::SameLine();
				}

				// 繧｢繧､繧ｳ繝ｳ
				ImGui::Text("%s", icon);
				ImGui::SameLine();

				// 繝ｪ繝阪・繝繝｢繝ｼ繝・
				if (isRenaming) {
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::InputText("##rename", renameBuffer_, sizeof(renameBuffer_),
						ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
						// Enter謚ｼ荳九〒繝ｪ繝阪・繝遒ｺ螳・
						if (strlen(renameBuffer_) > 0) {
							obj->SetName(renameBuffer_);
							consoleMessages_.push_back("[Editor] Renamed to: " + std::string(renameBuffer_));
						}
						renamingObject_ = nullptr;
					}
					// 蛻晏屓繝輔か繝ｼ繧ｫ繧ｹ險ｭ螳・
					if (ImGui::IsItemDeactivated() || (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())) {
						renamingObject_ = nullptr;
					}
					// 譛蛻昴・繝輔Ξ繝ｼ繝縺ｧ繝輔か繝ｼ繧ｫ繧ｹ
					if (ImGui::IsWindowAppearing() || (renamingObject_ == obj && !ImGui::IsItemActive())) {
						ImGui::SetKeyboardFocusHere(-1);
					}
				} else {
					// 騾壼ｸｸ陦ｨ遉ｺ
					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
						ImGuiTreeNodeFlags_NoTreePushOnOpen |
						ImGuiTreeNodeFlags_SpanAvailWidth;
					if (selectedObject_ == obj) {
						flags |= ImGuiTreeNodeFlags_Selected;
					}

					ImGui::TreeNodeEx(obj->GetName().c_str(), flags);

					// 繧ｷ繝ｳ繧ｰ繝ｫ繧ｯ繝ｪ繝・け縺ｧ驕ｸ謚橸ｼ九ヵ繧ｩ繝ｼ繧ｫ繧ｹ
					if (ImGui::IsItemClicked() && !ImGui::IsMouseDoubleClicked(0)) {
						selectedObject_ = obj;
						FocusOnObject(obj);
					}

					// 繝繝悶Ν繧ｯ繝ｪ繝・け縺ｧ繝ｪ繝阪・繝繝｢繝ｼ繝蛾幕蟋・
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
						renamingObject_ = obj;
						strncpy_s(renameBuffer_, obj->GetName().c_str(), sizeof(renameBuffer_) - 1);
						renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
					}
				}

				// 蜿ｳ繧ｯ繝ｪ繝・け繝｡繝九Η繝ｼ
				if (ImGui::BeginPopupContextItem()) {
					if (ImGui::MenuItem("Rename", "F2")) {
						renamingObject_ = obj;
						strncpy_s(renameBuffer_, obj->GetName().c_str(), sizeof(renameBuffer_) - 1);
						renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
					}
					if (ImGui::MenuItem("Focus", "F")) {
						FocusOnObject(obj);
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Delete", "DEL")) {
						if (gameObjects_) {
							for (auto it = gameObjects_->begin(); it != gameObjects_->end(); ++it) {
								if (it->get() == obj) {
									consoleMessages_.push_back("[Editor] Deleted object: " + obj->GetName());
									expandedObjects_.erase(obj);
									gameObjects_->erase(it);
									if (selectedObject_ == obj) {
										selectedObject_ = nullptr;
									}
									if (renamingObject_ == obj) {
										renamingObject_ = nullptr;
									}
									break;
								}
							}
						}
					}
					ImGui::EndPopup();
				}

				// 繧､繝ｳ繝ｩ繧､繝ｳ螻暮幕・啜ransform諠・ｱ
				if (isExpanded) {
					ImGui::Indent(20.0f);

					auto& transform = obj->GetTransform();

					// 繧ｮ繧ｺ繝｢謫堺ｽ應ｸｭ縺ｯ邱ｨ髮・ｒ辟｡蜉ｹ蛹厄ｼ育ｫｶ蜷医ｒ髦ｲ縺撰ｼ・
					bool isGizmoActive = gizmoSystem_.IsUsing() && obj == selectedObject_;
					if (isGizmoActive) {
						ImGui::BeginDisabled();
					}

					// 繝ｭ繝ｼ繧ｫ繝ｫ蠎ｧ讓吶ｒ菴ｿ逕ｨ・医ぐ繧ｺ繝｢縺ｨ邨ｱ荳・・
					Vector3 pos = transform.GetLocalPosition();
					Quaternion rot = transform.GetLocalRotation();
					Vector3 scale = transform.GetLocalScale();

					// 蝗櫁ｻ｢繧偵が繧､繝ｩ繝ｼ隗偵↓螟画鋤・・uaternion -> Euler・・
					float pitch, yaw, roll;
					float qx = rot.GetX(), qy = rot.GetY(), qz = rot.GetZ(), qw = rot.GetW();

					// Roll (X霆ｸ蝗櫁ｻ｢)
					float sinr_cosp = 2.0f * (qw * qx + qy * qz);
					float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
					roll = std::atan2(sinr_cosp, cosr_cosp);

					// Pitch (Y霆ｸ蝗櫁ｻ｢)
					float sinp = 2.0f * (qw * qy - qz * qx);
					if (std::abs(sinp) >= 1.0f)
						pitch = std::copysign(3.14159265f / 2.0f, sinp);
					else
						pitch = std::asin(sinp);

					// Yaw (Z霆ｸ蝗櫁ｻ｢)
					float siny_cosp = 2.0f * (qw * qz + qx * qy);
					float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
					yaw = std::atan2(siny_cosp, cosy_cosp);

					float euler[3] = { roll * 57.2958f, pitch * 57.2958f, yaw * 57.2958f };

					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

					// Position・医ラ繝ｩ繝・げ・・trl+繧ｯ繝ｪ繝・け縺ｧ逶ｴ謗･蜈･蜉幢ｼ・
					float posArr[3] = { pos.GetX(), pos.GetY(), pos.GetZ() };
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Pos", posArr, 0.1f, 0.0f, 0.0f, "%.2f")) {
						transform.SetLocalPosition(Vector3(posArr[0], posArr[1], posArr[2]));
					}

					// Rotation・医ラ繝ｩ繝・げ・・trl+繧ｯ繝ｪ繝・け縺ｧ逶ｴ謗･蜈･蜉幢ｼ・
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Rot", euler, 1.0f, 0.0f, 0.0f, "%.1f")) {
						// Euler隗抵ｼ亥ｺｦ・峨°繧渦uaternion縺ｸ螟画鋤
						float radX = euler[0] * 0.0174533f;
						float radY = euler[1] * 0.0174533f;
						float radZ = euler[2] * 0.0174533f;
						transform.SetLocalRotation(Quaternion::RotationRollPitchYaw(radX, radY, radZ));
					}

					// Scale・医ラ繝ｩ繝・げ・・trl+繧ｯ繝ｪ繝・け縺ｧ逶ｴ謗･蜈･蜉幢ｼ・
					float scaleArr[3] = { scale.GetX(), scale.GetY(), scale.GetZ() };
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Scale", scaleArr, 0.01f, 0.001f, 100.0f, "%.3f")) {
						transform.SetLocalScale(Vector3(scaleArr[0], scaleArr[1], scaleArr[2]));
					}

					ImGui::PopStyleColor();

tttttttt}

tttttif (auto* audioSource = obj->GetComponent<AudioSource>()) {
tttImGui::Spacing();
tttImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
tttImGui::Text("AudioSource");
tttImGui::PopStyleColor();

tttImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

ttt// クリップパス
tttchar clipPathBuf[256] = {};
tttstrncpy_s(clipPathBuf, audioSource->GetClipPath().c_str(), sizeof(clipPathBuf) - 1);
tttImGui::SetNextItemWidth(120.0f);
tttif (ImGui::InputText("Clip", clipPathBuf, sizeof(clipPathBuf))) {
ttttttttttttttttOPENFILENAMEA ofn = {};
tttttttofn.lStructSize = sizeof(ofn);
tttttttofn.lpstrFile = fileName;
tttttttofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
ttttttttttttttt}
ttt}

ttt// 音量
tttfloat volume = audioSource->GetVolume();
tttImGui::SetNextItemWidth(120.0f);
tttif (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
tttttttttttttttttttaudioSource->SetLoop(loop);
ttt}

ttt// PlayOnAwake
tttImGui::SameLine();
tttbool playOnAwake = audioSource->GetPlayOnAwake();
tttif (ImGui::Checkbox("PlayOnAwake", &playOnAwake)) {
tttttttttttttttttttaudioSource->Set3D(is3D);
ttt}

tttif (is3D) {
tttttttfloat maxDist = audioSource->GetMaxDistance();
tttttttif (ImGui::DragFloat("MinDist", &minDist, 0.1f, 0.1f, 1000.0f)) {
ttttaudioSource->SetMinDistance(minDist);
tttttttImGui::SameLine();
tttttttif (ImGui::DragFloat("MaxDist", &maxDist, 0.1f, 0.1f, 1000.0f)) {
ttttaudioSource->SetMaxDistance(maxDist);
tttttttttttttttttttif (ImGui::Button("Stop##preview")) {
ttttaudioSource->Stop();
ttttttttttif (ImGui::Button("Preview##play")) {
ttttaudioSource->Play();
tttttttttttt}

tttttif (obj->GetComponent<AudioListener>()) {
tttImGui::Spacing();
tttImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.7f, 1.0f));
tttImGui::Text("AudioListener (Active)");
tttImGui::PopStyleColor();
tttttImGui::Unindent(20.0f);
				}

				ImGui::PopID();
			}
		}
		else {
			ImGui::TextDisabled("(no objects)");
		}

		// DEL繧ｭ繝ｼ縺ｧ驕ｸ謚樔ｸｭ縺ｮ繧ｪ繝悶ず繧ｧ繧ｯ繝医ｒ蜑企勁
		if (selectedObject_ && !renamingObject_ && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
			if (gameObjects_) {
				for (auto it = gameObjects_->begin(); it != gameObjects_->end(); ++it) {
					if (it->get() == selectedObject_) {
						consoleMessages_.push_back("[Editor] Deleted object (DEL): " + selectedObject_->GetName());
						expandedObjects_.erase(selectedObject_);
						gameObjects_->erase(it);
						selectedObject_ = nullptr;
						break;
					}
				}
			}
		}

		// F2繧ｭ繝ｼ縺ｧ繝ｪ繝阪・繝繝｢繝ｼ繝蛾幕蟋・
		if (selectedObject_ && !renamingObject_ && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2)) {
			renamingObject_ = selectedObject_;
			strncpy_s(renameBuffer_, selectedObject_->GetName().c_str(), sizeof(renameBuffer_) - 1);
			renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
		}

		// Escape繧ｭ繝ｼ縺ｧ繝ｪ繝阪・繝繝｢繝ｼ繝峨ｒ繧ｭ繝｣繝ｳ繧ｻ繝ｫ
		if (renamingObject_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			renamingObject_ = nullptr;
		}

		// 繧ｦ繧｣繝ｳ繝峨え蜈ｨ菴薙ｒ繝峨Ο繝・・繧ｿ繝ｼ繧ｲ繝・ヨ縺ｫ・郁レ譎ｯ繧ｨ繝ｪ繧｢・・
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

		// 繝峨Ο繝・・繧ｾ繝ｼ繝ｳ縺ｮ繝偵Φ繝郁｡ｨ遉ｺ
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

		// Models繝輔か繝ｫ繝繧偵せ繧ｭ繝｣繝ｳ
		if (ImGui::TreeNode("Models")) {
			// 繝ｪ繝輔Ξ繝・す繝･繝懊ち繝ｳ
			if (ImGui::SmallButton("Refresh")) {
				RefreshModelPaths();
				consoleMessages_.push_back("[Editor] Model list refreshed");
			}
			ImGui::Separator();

			// 蛻晏屓縺ｾ縺溘・繝ｪ繝輔Ξ繝・す繝･譎ゅ↓繧ｹ繧ｭ繝｣繝ｳ
			if (cachedModelPaths_.empty()) {
				RefreshModelPaths();
			}

			// 繝｢繝・Ν繝ｪ繧ｹ繝医ｒ陦ｨ遉ｺ
			for (size_t i = 0; i < cachedModelPaths_.size(); ++i) {
				const auto& modelPath = cachedModelPaths_[i];
				std::filesystem::path p(modelPath);
				std::string filename = p.filename().string();

				ImGui::PushID(static_cast<int>(i));
				
				// 繧｢繧､繧ｳ繝ｳ陦ｨ遉ｺ・医ヵ繧｡繧､繝ｫ諡｡蠑ｵ蟄舌↓蠢懊§縺ｦ・・
			std::string ext = p.extension().string();

			// OBJ繝輔ぃ繧､繝ｫ縺ｯ繧ｹ繧ｭ繝ｳ繝｡繝・す繝･髱槫ｯｾ蠢懊↑縺ｮ縺ｧ繧ｹ繧ｭ繝・・
			if (ext == ".obj") {
				ImGui::PopID();
				continue;
			}

			const char* icon = "逃"; // 繝・ヵ繧ｩ繝ｫ繝医い繧､繧ｳ繝ｳ
			if (ext == ".gltf" || ext == ".glb") icon = "耳";
			else if (ext == ".fbx") icon = "塙";
				
				ImGui::Text("%s", icon);
				ImGui::SameLine();
				
				bool selected = false;
				if (ImGui::Selectable(filename.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
					// 繝繝悶Ν繧ｯ繝ｪ繝・け縺ｧ繧ｷ繝ｼ繝ｳ縺ｫ霑ｽ蜉
					if (ImGui::IsMouseDoubleClicked(0)) {
						HandleModelDragDropByIndex(i);
					}
				}

				// 繝峨Λ繝・げ繧ｽ繝ｼ繧ｹ險ｭ螳・
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
					ImGui::SetDragDropPayload("MODEL_INDEX", &i, sizeof(size_t));
					ImGui::Text("識 Drag: %s", filename.c_str());
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

		// 繝・く繧ｹ繝亥・蜉帑ｸｭ縺ｯ繝帙ャ繝医く繝ｼ繧堤┌蜉ｹ蛹・
		if (io.WantTextInput) return;

		// F5: Play/Pause蛻・ｊ譖ｿ縺・
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

		// Escape: Stop・亥・逕滉ｸｭ/荳譎ょ●豁｢荳ｭ・峨∪縺溘・驕ｸ謚櫁ｧ｣髯､・育ｷｨ髮・Δ繝ｼ繝会ｼ・
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
			if (editorMode_ != EditorMode::Edit) {
				Stop();
			} else {
				// 邱ｨ髮・Δ繝ｼ繝峨〒縺ｯ驕ｸ謚槭ｒ繧ｯ繝ｪ繧｢
				selectedObject_ = nullptr;
			}
		}

		// F1: Scene View陦ｨ遉ｺ繝医げ繝ｫ
		if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
			showSceneView_ = !showSceneView_;
		}

		// F2: Game View陦ｨ遉ｺ繝医げ繝ｫ
		if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
			showGameView_ = !showGameView_;
		}

		// Q: 遘ｻ蜍輔ぐ繧ｺ繝｢
		if (ImGui::IsKeyPressed(ImGuiKey_Q, false) && !io.KeyCtrl) {
			gizmoSystem_.SetOperation(GizmoOperation::Translate);
			consoleMessages_.push_back("[Editor] Gizmo: Translate");
		}

		// E: 蝗櫁ｻ｢繧ｮ繧ｺ繝｢
		if (ImGui::IsKeyPressed(ImGuiKey_E, false) && !io.KeyCtrl) {
			gizmoSystem_.SetOperation(GizmoOperation::Rotate);
			consoleMessages_.push_back("[Editor] Gizmo: Rotate");
		}

		// R: 繧ｹ繧ｱ繝ｼ繝ｫ繧ｮ繧ｺ繝｢・・trl+R縺ｯ繝ｬ繧､繧｢繧ｦ繝医Μ繧ｻ繝・ヨ逕ｨ縺ｪ縺ｮ縺ｧ髯､螟厄ｼ・
		if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !io.KeyCtrl && !io.KeyShift) {
			gizmoSystem_.SetOperation(GizmoOperation::Scale);
			consoleMessages_.push_back("[Editor] Gizmo: Scale");
		}

		// G: Local/World蛻・ｊ譖ｿ縺・
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

		// F10: Step・井ｸ譎ょ●豁｢荳ｭ縺ｮ縺ｿ・・
		if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) {
			if (editorMode_ == EditorMode::Pause) {
				Step();
			}
		}

		// Ctrl+Shift+R: 繝ｬ繧､繧｢繧ｦ繝医Μ繧ｻ繝・ヨ
		if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_R, false)) {
			dockingLayoutInitialized_ = false;
			consoleMessages_.push_back("[Editor] Layout reset");
		}

		// Shift+F5: 蛛懈ｭ｢・・S繧ｹ繧ｿ繧､繝ｫ・・
		if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
			if (editorMode_ != EditorMode::Edit) {
				Stop();
			}
		}

		// Ctrl+Z: Undo・医ぐ繧ｺ繝｢謫堺ｽ懊ｒ蜈・↓謌ｻ縺呻ｼ・
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			PerformUndo();
		}

		// Ctrl+S: 繧ｷ繝ｼ繝ｳ菫晏ｭ・
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			SaveScene("assets/scenes/default_scene.json");
		}
	}

	// Undo螻･豁ｴ縺ｫ霑ｽ蜉
	void EditorUI::PushUndoSnapshot(const TransformSnapshot& snapshot) {
		undoStack_.push(snapshot);
		consoleMessages_.push_back("[Editor] Transform change recorded");
	}

	// Undo螳溯｡・
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

	// 繧ｷ繝ｼ繝ｳ菫晏ｭ・
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

	// 繧ｷ繝ｼ繝ｳ繝ｭ繝ｼ繝・
	void EditorUI::LoadScene(const std::string& filepath) {
		if (!gameObjects_) {
			consoleMessages_.push_back("[Editor] Error: No game objects container");
			return;
		}

		if (SceneSerializer::LoadScene(filepath, *gameObjects_)) {
			consoleMessages_.push_back("[Editor] Scene loaded: " + filepath);
			// 繝ｭ繝ｼ繝牙ｾ後∵怙蛻昴・繧ｪ繝悶ず繧ｧ繧ｯ繝医ｒ驕ｸ謚・
			if (!gameObjects_->empty()) {
				selectedObject_ = (*gameObjects_)[0].get();
			}
		}
		else {
			consoleMessages_.push_back("[Editor] Failed to load scene: " + filepath);
		}
	}

	// 繝｢繝・ΝD&D蜃ｦ逅・ｼ医ヱ繧ｹ縺九ｉ・・
	void EditorUI::HandleModelDragDrop(const std::string& modelPath) {
		if (!gameObjects_ || !resourceManager_) {
			consoleMessages_.push_back("[Editor] Error: Cannot create object - missing dependencies");
			return;
		}

		// 驕・ｻｶ繝ｭ繝ｼ繝峨く繝･繝ｼ縺ｫ霑ｽ蜉
		pendingModelLoads_.push_back(modelPath);
		consoleMessages_.push_back("[Editor] Model queued for loading: " + modelPath);
	}

	// 繝｢繝・ΝD&D蜃ｦ逅・ｼ医う繝ｳ繝・ャ繧ｯ繧ｹ縺九ｉ・・
	void EditorUI::HandleModelDragDropByIndex(size_t modelIndex) {
		if (modelIndex < cachedModelPaths_.size()) {
			HandleModelDragDrop(cachedModelPaths_[modelIndex]);
		}
		else {
			consoleMessages_.push_back("[Editor] Error: Invalid model index");
		}
	}

	// 繝｢繝・Ν繝代せ繧偵Μ繝輔Ξ繝・す繝･
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

	// 驕・ｻｶ繝ｭ繝ｼ繝牙・逅・
	void EditorUI::ProcessPendingLoads() {
		if (pendingModelLoads_.empty()) return;
		if (!gameObjects_ || !resourceManager_) return;

		// 蜷・Δ繝・Ν繧貞句挨縺ｫ蜃ｦ逅・ｼ郁､・焚繝｢繝・Ν繧・縺､縺ｮ繧｢繝・・繝ｭ繝ｼ繝峨さ繝ｳ繝・く繧ｹ繝医〒蜃ｦ逅・☆繧九→謠冗判繝舌げ縺檎匱逕溘☆繧九◆繧・ｼ・
	for (const auto& modelPath : pendingModelLoads_) {
			consoleMessages_.push_back("[Editor] Loading model: " + modelPath);

			// 繝｢繝・Ν蜷阪ｒ蜿門ｾ暦ｼ域僑蠑ｵ蟄舌↑縺暦ｼ・
		std::filesystem::path path(modelPath);
		std::string modelName = path.stem().string();

		// 縺薙・繝｢繝・Ν蟆ら畑縺ｮ繧｢繝・・繝ｭ繝ｼ繝峨さ繝ｳ繝・く繧ｹ繝・
		resourceManager_->BeginUpload();

		// 繝｢繝・Ν繧偵Ο繝ｼ繝・
		auto* modelData = resourceManager_->LoadSkinnedModel(modelPath);

		// 繧｢繝・・繝ｭ繝ｼ繝牙ｮ御ｺ・
		resourceManager_->EndUpload();

			if (!modelData) {
				consoleMessages_.push_back("[Editor] ERROR: Failed to load model: " + modelPath);
				continue;
			}

			consoleMessages_.push_back("[Editor] Model loaded successfully");

			// GameObject繧堤函謌・
			auto newObject = std::make_unique<GameObject>(modelName);

			// AnimatorComponent繧貞・縺ｫ霑ｽ蜉・・kinnedMeshRenderer::Awake()縺ｧ繝ｪ繝ｳ繧ｯ縺ｧ縺阪ｋ繧医≧縺ｫ・・
			auto* animator = newObject->AddComponent<AnimatorComponent>();
			if (modelData->skeleton) {
				animator->Initialize(modelData->skeleton, modelData->animations);
				if (!modelData->animations.empty()) {
					std::string animName = modelData->animations[0]->GetName();
					animator->Play(animName, true);
					consoleMessages_.push_back("[Editor] Playing animation: " + animName);
				}
			}

			// SkinnedMeshRenderer繧定ｿｽ蜉・・nimatorComponent縺梧里縺ｫ蟄伜惠縺吶ｋ縺ｮ縺ｧAwake()縺ｧ繝ｪ繝ｳ繧ｯ縺輔ｌ繧具ｼ・
			auto* renderer = newObject->AddComponent<SkinnedMeshRenderer>();
			renderer->SetModel(modelPath);  // 縺ｾ縺壹ヱ繧ｹ繧定ｨｭ螳・
			renderer->SetModel(modelData);   // 谺｡縺ｫ螳滄圀縺ｮ繝｢繝・Ν繝・・繧ｿ繧定ｨｭ螳・

			// 驕ｸ謚樒憾諷九↓縺吶ｋ
			selectedObject_ = newObject.get();

			// GameObjects繝ｪ繧ｹ繝医↓霑ｽ蜉
			gameObjects_->push_back(std::move(newObject));

			// 驥崎ｦ・ 繧ｳ繝ｳ繝昴・繝阪Φ繝医・Start()繧貞他繧薙〒蛻晄悄蛹・
			// ・亥・襍ｷ蜍墓凾縺ｯScene::ProcessPendingStarts()縺ｧ蜻ｼ縺ｰ繧後ｋ縺後．&D譎ゅ・謇句虚縺ｧ蜻ｼ縺ｶ蠢・ｦ√′縺ゅｋ・・
			if (scene_) {
				scene_->StartGameObject(selectedObject_);
			}

			// 繧ｫ繝｡繝ｩ繧偵Δ繝・Ν縺ｫ繝輔か繝ｼ繧ｫ繧ｹ・域眠隕剰ｿｽ蜉縺ｪ縺ｮ縺ｧ隗貞ｺｦ繧ゅΜ繧ｻ繝・ヨ・・
			FocusOnNewObject(selectedObject_);

			consoleMessages_.push_back("[Editor] Created object: " + modelName);
	}

	// 繧ｭ繝･繝ｼ繧偵け繝ｪ繧｢
		pendingModelLoads_.clear();
	}

	// 繧ｪ繝悶ず繧ｧ繧ｯ繝医↓繧ｫ繝｡繝ｩ繧偵ヵ繧ｩ繝ｼ繧ｫ繧ｹ・医ヰ繧ｦ繝ｳ繝・ぅ繝ｳ繧ｰ繝懊ャ繧ｯ繧ｹ縺九ｉ霍晞屬繧定・蜍戊ｨ育ｮ暦ｼ・
	void EditorUI::FocusOnObject(GameObject* obj) {
		if (!obj) return;

		// 繧ｪ繝悶ず繧ｧ繧ｯ繝医・繝ｯ繝ｼ繝ｫ繝芽｡悟・縺ｨ繧ｹ繧ｱ繝ｼ繝ｫ繧貞叙蠕・
		auto& transform = obj->GetTransform();
		Matrix4x4 worldMatrix = transform.GetWorldMatrix();
		float m[16];
		worldMatrix.ToFloatArray(m);
		Vector3 targetPos(m[12], m[13], m[14]);
		Vector3 worldScale = transform.GetScale();

		// 繝・ヵ繧ｩ繝ｫ繝郁ｷ晞屬
		float distance = 5.0f;

		// SkinnedMeshRenderer縺後≠繧句ｴ蜷医・繝舌え繝ｳ繝・ぅ繝ｳ繧ｰ繝懊ャ繧ｯ繧ｹ縺九ｉ霍晞屬繧定ｨ育ｮ・
		auto* renderer = obj->GetComponent<SkinnedMeshRenderer>();
		if (renderer && renderer->GetModelData()) {
			auto* modelData = renderer->GetModelData();
			if (!modelData->meshes.empty()) {
				// 蜈ｨ繝｡繝・す繝･縺ｮ繝舌え繝ｳ繝・ぅ繝ｳ繧ｰ繝懊ャ繧ｯ繧ｹ繧堤ｵｱ蜷・
				Vector3 boundsMin = modelData->meshes[0].GetBoundsMin();
				Vector3 boundsMax = modelData->meshes[0].GetBoundsMax();

				for (size_t i = 1; i < modelData->meshes.size(); ++i) {
					Vector3 meshMin = modelData->meshes[i].GetBoundsMin();
					Vector3 meshMax = modelData->meshes[i].GetBoundsMax();
					boundsMin.SetX((std::min)(boundsMin.GetX(), meshMin.GetX()));
					boundsMin.SetY((std::min)(boundsMin.GetY(), meshMin.GetY()));
					boundsMin.SetZ((std::min)(boundsMin.GetZ(), meshMin.GetZ()));
					boundsMax.SetX((std::max)(boundsMax.GetX(), meshMax.GetX()));
					boundsMax.SetY((std::max)(boundsMax.GetY(), meshMax.GetY()));
					boundsMax.SetZ((std::max)(boundsMax.GetZ(), meshMax.GetZ()));
				}

				// 繝ｭ繝ｼ繧ｫ繝ｫ縺ｮ荳ｭ蠢・→繧ｵ繧､繧ｺ繧定ｨ育ｮ・
				Vector3 localCenter = (boundsMin + boundsMax) * 0.5f;
				Vector3 localSize = boundsMax - boundsMin;

				// 繝ｯ繝ｼ繝ｫ繝峨せ繧ｱ繝ｼ繝ｫ繧帝←逕ｨ
				Vector3 worldSize(
					localSize.GetX() * worldScale.GetX(),
					localSize.GetY() * worldScale.GetY(),
					localSize.GetZ() * worldScale.GetZ()
				);
				float maxDimension = (std::max)({ worldSize.GetX(), worldSize.GetY(), worldSize.GetZ() });

				// 繧ｿ繝ｼ繧ｲ繝・ヨ菴咲ｽｮ繧偵Ρ繝ｼ繝ｫ繝峨せ繧ｱ繝ｼ繝ｫ驕ｩ逕ｨ縺励◆荳ｭ蠢・↓隱ｿ謨ｴ
				Vector3 scaledCenter(
					localCenter.GetX() * worldScale.GetX(),
					localCenter.GetY() * worldScale.GetY(),
					localCenter.GetZ() * worldScale.GetZ()
				);
				targetPos = targetPos + scaledCenter;

				// 繧ｫ繝｡繝ｩ霍晞屬繧定ｨ育ｮ暦ｼ医Δ繝・Ν蜈ｨ菴薙′隕九∴繧九ｈ縺・↓・・
				distance = maxDimension * 1.5f;
				distance = (std::max)(distance, 2.0f);  // 譛蟆剰ｷ晞屬
			}
		}

		editorCamera_.FocusOn(targetPos, distance, false);
	}

	// 繧ｪ繝悶ず繧ｧ繧ｯ繝医↓繧ｫ繝｡繝ｩ繧偵ヵ繧ｩ繝ｼ繧ｫ繧ｹ・域眠隕剰ｿｽ蜉譎ら畑縲∬ｧ貞ｺｦ繝ｪ繧ｻ繝・ヨ・・
	void EditorUI::FocusOnNewObject(GameObject* obj) {
		if (!obj) return;

		// 繧ｪ繝悶ず繧ｧ繧ｯ繝医・繝ｯ繝ｼ繝ｫ繝芽｡悟・縺ｨ繧ｹ繧ｱ繝ｼ繝ｫ繧貞叙蠕・
		auto& transform = obj->GetTransform();
		Matrix4x4 worldMatrix = transform.GetWorldMatrix();
		float m[16];
		worldMatrix.ToFloatArray(m);
		Vector3 targetPos(m[12], m[13], m[14]);
		Vector3 worldScale = transform.GetScale();

		// 繝・ヵ繧ｩ繝ｫ繝郁ｷ晞屬
		float distance = 5.0f;

		// SkinnedMeshRenderer縺後≠繧句ｴ蜷医・繝舌え繝ｳ繝・ぅ繝ｳ繧ｰ繝懊ャ繧ｯ繧ｹ縺九ｉ霍晞屬繧定ｨ育ｮ・
		auto* renderer = obj->GetComponent<SkinnedMeshRenderer>();
		if (renderer && renderer->GetModelData()) {
			auto* modelData = renderer->GetModelData();
			if (!modelData->meshes.empty()) {
				// 蜈ｨ繝｡繝・す繝･縺ｮ繝舌え繝ｳ繝・ぅ繝ｳ繧ｰ繝懊ャ繧ｯ繧ｹ繧堤ｵｱ蜷・
				Vector3 boundsMin = modelData->meshes[0].GetBoundsMin();
				Vector3 boundsMax = modelData->meshes[0].GetBoundsMax();

				for (size_t i = 1; i < modelData->meshes.size(); ++i) {
					Vector3 meshMin = modelData->meshes[i].GetBoundsMin();
					Vector3 meshMax = modelData->meshes[i].GetBoundsMax();
					boundsMin.SetX((std::min)(boundsMin.GetX(), meshMin.GetX()));
					boundsMin.SetY((std::min)(boundsMin.GetY(), meshMin.GetY()));
					boundsMin.SetZ((std::min)(boundsMin.GetZ(), meshMin.GetZ()));
					boundsMax.SetX((std::max)(boundsMax.GetX(), meshMax.GetX()));
					boundsMax.SetY((std::max)(boundsMax.GetY(), meshMax.GetY()));
					boundsMax.SetZ((std::max)(boundsMax.GetZ(), meshMax.GetZ()));
				}

				// 繝ｭ繝ｼ繧ｫ繝ｫ縺ｮ荳ｭ蠢・→繧ｵ繧､繧ｺ繧定ｨ育ｮ・
				Vector3 localCenter = (boundsMin + boundsMax) * 0.5f;
				Vector3 localSize = boundsMax - boundsMin;

				// 繝ｯ繝ｼ繝ｫ繝峨せ繧ｱ繝ｼ繝ｫ繧帝←逕ｨ
				Vector3 worldSize(
					localSize.GetX() * worldScale.GetX(),
					localSize.GetY() * worldScale.GetY(),
					localSize.GetZ() * worldScale.GetZ()
				);
				float maxDimension = (std::max)({ worldSize.GetX(), worldSize.GetY(), worldSize.GetZ() });

				// 繧ｿ繝ｼ繧ｲ繝・ヨ菴咲ｽｮ繧偵Ρ繝ｼ繝ｫ繝峨せ繧ｱ繝ｼ繝ｫ驕ｩ逕ｨ縺励◆荳ｭ蠢・↓隱ｿ謨ｴ
				Vector3 scaledCenter(
					localCenter.GetX() * worldScale.GetX(),
					localCenter.GetY() * worldScale.GetY(),
					localCenter.GetZ() * worldScale.GetZ()
				);
				targetPos = targetPos + scaledCenter;

				// 繧ｫ繝｡繝ｩ霍晞屬繧定ｨ育ｮ暦ｼ医Δ繝・Ν蜈ｨ菴薙′隕九∴繧九ｈ縺・↓・・
				distance = maxDimension * 1.5f;
				distance = (std::max)(distance, 2.0f);  // 譛蟆剰ｷ晞屬
			}
		}

		// 譁ｰ隕剰ｿｽ蜉譎ゅ・隗貞ｺｦ繧偵Μ繧ｻ繝・ヨ・域万繧∽ｸ翫°繧会ｼ・
		editorCamera_.FocusOn(targetPos, distance, true);
	}

} // namespace UnoEngine
