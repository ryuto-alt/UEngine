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
#include "../../Engine/Core/CameraComponent.h"
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
#include <commdlg.h>
#include <filesystem>

namespace UnoEngine {

	void EditorUI::Initialize(GraphicsDevice* graphics) {
		// RenderTexture setup (SRVインデックス 3と4を使用) - 16:9 aspect ratio
		gameViewTexture_.Create(graphics, 1280, 720, 3);
		sceneViewTexture_.Create(graphics, 1280, 720, 4);

		// Scene View用カメラの初期化
		sceneViewCamera_.SetPerspective(
			60.0f * 0.0174533f,  // FOV 60度
			16.0f / 9.0f,        // アスペクト比
			0.1f,                // Near clip
			1000.0f              // Far clip
		);
		sceneViewCamera_.SetPosition(Vector3(0.0f, 5.0f, -10.0f));
		// 少し下を向く（原点を見る感じ）
		sceneViewCamera_.SetRotation(Quaternion::RotationRollPitchYaw(0.3f, 0.0f, 0.0f));

		// EditorCameraにScene View用カメラを設定
		editorCamera_.SetCamera(&sceneViewCamera_);

		// ギズモシステム初期化
		gizmoSystem_.Initialize();

		// エディタカメラ設定を読み込み
		editorCamera_.LoadSettings();

		// Console初期ログ
		consoleMessages_.push_back("[System] UnoEngine Editor Initialized");
		consoleMessages_.push_back("[Info] Press ~ to toggle console");
		consoleMessages_.push_back("[Info] Q: Translate, E: Rotate, R: Scale");
	}

	void EditorUI::Render(const EditorContext& context) {
		// ImGuizmoフレーム開始
		ImGuizmo::BeginFrame();

		// Game Camera（Main Camera）を設定
		if (context.camera) {
			gameCamera_ = context.camera;
		}

		// Scene Viewのアスペクト比を更新
		if (desiredSceneViewWidth_ > 0 && desiredSceneViewHeight_ > 0) {
			float aspect = static_cast<float>(desiredSceneViewWidth_) / static_cast<float>(desiredSceneViewHeight_);
			sceneViewCamera_.SetPerspective(
				60.0f * 0.0174533f,  // FOV 60度
				aspect,
				0.1f,
				1000.0f
			);
		}

		// アニメーションシステムを設定
		if (context.animationSystem) {
			animationSystem_ = context.animationSystem;
		}

		// 3Dオーディオ：リスナー位置を更新
		if (AudioListener::GetInstance()) {
			if (editorMode_ == EditorMode::Play && gameCamera_) {
				// Playモード中はGame Camera位置をリスナー位置として使用
				AudioListener::GetInstance()->SetEditorOverridePosition(
					gameCamera_->GetPosition());
				AudioListener::GetInstance()->SetEditorOverrideOrientation(
					gameCamera_->GetForward(),
					gameCamera_->GetUp());
			} else if (previewingAudioSource_ && previewingAudioSource_->IsPlaying() &&
				previewingAudioSource_->Is3D()) {
				// プレビュー中はScene Viewカメラ位置・向きを継続的に更新
				AudioListener::GetInstance()->SetEditorOverridePosition(
					sceneViewCamera_.GetPosition());
				AudioListener::GetInstance()->SetEditorOverrideOrientation(
					sceneViewCamera_.GetForward(),
					sceneViewCamera_.GetUp());
			}
		}

		// プレビュー終了時の処理
		if (previewingAudioSource_ && !previewingAudioSource_->IsPlaying()) {
			if (AudioListener::GetInstance()) {
				AudioListener::GetInstance()->ClearEditorOverride();
			}
			previewingAudioSource_ = nullptr;
			// Playモードでなければエディタ用リスナーも解放
			if (editorMode_ == EditorMode::Edit) {
				editorAudioListener_.reset();
			}
		}

		// ホットキー処理
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

		// エディタカメラの更新（Edit/Pauseモードのみ）
		if (editorMode_ != EditorMode::Play) {
			float deltaTime = ImGui::GetIO().DeltaTime;
			editorCamera_.SetMovementEnabled(true);
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
			// AudioSystemのポーズ状態をリセット
			if (audioSystem_ && audioSystem_->IsPaused()) {
				audioSystem_->ResumeAll();
			}

			// シーンにAudioListenerがない場合はエディタ用を作成
			// （3Dオーディオを機能させるため）
			if (!AudioListener::GetInstance()) {
				editorAudioListener_ = std::make_unique<AudioListener>();
				consoleMessages_.push_back("[Audio] Created AudioListener for Play mode");
			}
			// Playモードではエディタオーバーライドをクリア（GameObjectの位置を使う）
			if (AudioListener::GetInstance()) {
				AudioListener::GetInstance()->ClearEditorOverride();
			}

			// PlayOnAwakeのAudioSourceを再生
			int playCount = 0;
			if (gameObjects_) {
				for (auto& obj : *gameObjects_) {
					if (auto* audioSource = obj->GetComponent<AudioSource>()) {
						if (audioSource->GetPlayOnAwake()) {
							audioSource->Play();
							playCount++;
						}
					}
				}
			}
			consoleMessages_.push_back("[Editor] Play mode started (triggered " + std::to_string(playCount) + " audio sources)");
		}
		else if (editorMode_ == EditorMode::Pause) {
			editorMode_ = EditorMode::Play;
			// アニメーション再開
			if (animationSystem_) {
				animationSystem_->SetPlaying(true);
			}
			// オーディオ再開
			if (audioSystem_) {
				audioSystem_->ResumeAll();
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
			// オーディオ一時停止
			if (audioSystem_) {
				audioSystem_->PauseAll();
			}
			consoleMessages_.push_back("[Editor] Paused");
		}
	}

	void EditorUI::Stop() {
		if (editorMode_ != EditorMode::Edit) {
			editorMode_ = EditorMode::Edit;
			// マウスロック解除
			if (gameViewMouseLocked_) {
				gameViewMouseLocked_ = false;
				while (ShowCursor(TRUE) < 0);
			}
			// アニメーション停止
			if (animationSystem_) {
				animationSystem_->SetPlaying(false);
			}
			// AudioSystemのポーズ状態をリセット（Pause中にStopされた場合）
			if (audioSystem_ && audioSystem_->IsPaused()) {
				audioSystem_->ResumeAll();
			}
			// オーディオ停止
			int stoppedCount = 0;
			if (gameObjects_) {
				for (auto& obj : *gameObjects_) {
					if (auto* audioSource = obj->GetComponent<AudioSource>()) {
						audioSource->Stop();
						stoppedCount++;
					}
				}
			}
			// エディタ用AudioListenerを解放
			if (AudioListener::GetInstance()) {
				AudioListener::GetInstance()->ClearEditorOverride();
			}
			editorAudioListener_.reset();

			consoleMessages_.push_back("[Editor] Stopped - returned to Edit mode (stopped " + std::to_string(stoppedCount) + " audio sources)");
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
			// 注意: 最後にDockしたウィンドウがアクティブタブになる
			ImGui::DockBuilderDockWindow("Inspector", dock_left);
			ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
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

		// ウィンドウ全体のホバー状態を取得（画像以外の領域でも操作可能に）
		bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
		bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

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

			// ウィンドウ全体のホバー状態でカメラ操作を有効化
			editorCamera_.SetViewportHovered(windowHovered);
			editorCamera_.SetViewportFocused(windowFocused);

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

		// Game Viewのフォーカス状態を追跡
		gameViewFocused_ = ImGui::IsWindowFocused();
		gameViewHovered_ = ImGui::IsWindowHovered();

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

			// Playモード時のマウスロック＋FPS視点操作
			if (editorMode_ == EditorMode::Play) {
				ImGuiIO& io = ImGui::GetIO();
				bool imageHovered = ImGui::IsItemHovered();

				// Game View画像上で左クリックしたらマウスロック開始
				if (imageHovered && io.MouseClicked[0] && !gameViewMouseLocked_) {
					gameViewMouseLocked_ = true;
					GetCursorPos(&gameViewLockMousePos_);
					while (ShowCursor(FALSE) >= 0);

					// 現在のカメラの向きからyaw/pitchを初期化
					if (gameCamera_) {
						Vector3 forward = gameCamera_->GetForward();
						gameViewYaw_ = std::atan2(forward.GetX(), forward.GetZ());
						gameViewPitch_ = std::asin(-forward.GetY());
					}
				}

				// TABキーでマウスロック解除（Playモードは継続）
				if (gameViewMouseLocked_ && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
					gameViewMouseLocked_ = false;
					while (ShowCursor(TRUE) < 0);
				}

				// マウスロック中の視点操作とWASD移動
				if (gameViewMouseLocked_ && gameCamera_) {
					POINT currentPos;
					GetCursorPos(&currentPos);

					float deltaX = static_cast<float>(currentPos.x - gameViewLockMousePos_.x);
					float deltaY = static_cast<float>(currentPos.y - gameViewLockMousePos_.y);

					SetCursorPos(gameViewLockMousePos_.x, gameViewLockMousePos_.y);

					// 感度
					float sensitivity = editorCamera_.GetRotateSpeed() * io.DeltaTime;
					gameViewYaw_ += deltaX * sensitivity;
					gameViewPitch_ += deltaY * sensitivity;

					// ピッチ制限
					const float maxPitch = 1.5f;
					if (gameViewPitch_ > maxPitch) gameViewPitch_ = maxPitch;
					if (gameViewPitch_ < -maxPitch) gameViewPitch_ = -maxPitch;

					// カメラの向きを更新
					Quaternion rot = Quaternion::RotationRollPitchYaw(gameViewPitch_, gameViewYaw_, 0.0f);
					gameCamera_->SetRotation(rot);

					// WASD移動
					Vector3 forward = gameCamera_->GetForward();
					Vector3 right = Vector3::UnitY().Cross(forward).Normalize();

					// 水平面に投影
					Vector3 forwardXZ(forward.GetX(), 0.0f, forward.GetZ());
					if (forwardXZ.Length() > 0.001f) {
						forwardXZ = forwardXZ.Normalize();
					}

					Vector3 movement = Vector3::Zero();
					float moveSpeed = editorCamera_.GetMoveSpeed() * io.DeltaTime;

					if (ImGui::IsKeyDown(ImGuiKey_W)) movement = movement + forwardXZ;
					if (ImGui::IsKeyDown(ImGuiKey_S)) movement = movement - forwardXZ;
					if (ImGui::IsKeyDown(ImGuiKey_A)) movement = movement - right;
					if (ImGui::IsKeyDown(ImGuiKey_D)) movement = movement + right;
					if (ImGui::IsKeyDown(ImGuiKey_Space)) movement = movement + Vector3::UnitY();
					if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) movement = movement - Vector3::UnitY();

					if (movement.Length() > 0.001f) {
						movement = movement.Normalize() * moveSpeed;
						gameCamera_->SetPosition(gameCamera_->GetPosition() + movement);
					}
				}
			} else {
				// Playモード以外ではマウスロック解除
				if (gameViewMouseLocked_) {
					gameViewMouseLocked_ = false;
					while (ShowCursor(TRUE) < 0);
				}
			}
		}

		ImGui::End();
	}

	void EditorUI::RenderInspector(const EditorContext& context) {
		if (!showInspector_) return;

		ImGui::Begin("Inspector", &showInspector_);

		// 選択されたオブジェクトの情報を表示
		GameObject* selected = selectedObject_ ? selectedObject_ : context.player;

		if (selected) {
			ImGui::Text("Selected: %s", selected->GetName().c_str());
			ImGui::Separator();

			auto& transform = selected->GetTransform();
			auto pos = transform.GetLocalPosition();
			auto rot = transform.GetLocalRotation();
			auto scale = transform.GetLocalScale();

			ImGui::Text("Transform");
			ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.GetX(), pos.GetY(), pos.GetZ());
			ImGui::Text("Rotation: (%.2f, %.2f, %.2f, %.2f)",
				rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());
			ImGui::Text("Scale: (%.2f, %.2f, %.2f)", scale.GetX(), scale.GetY(), scale.GetZ());

			// LuaScriptComponentの表示
			auto* luaScript = selected->GetComponent<LuaScriptComponent>();
			if (luaScript) {
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
				ImGui::Text("Lua Script");
				ImGui::PopStyleColor();

				// スクリプト選択コンボボックス
				if (cachedScriptPaths_.empty()) {
					RefreshScriptPaths();
				}

				std::string currentScript = luaScript->GetScriptPath();
				int currentIndex = -1;
				for (size_t i = 0; i < cachedScriptPaths_.size(); ++i) {
					if (cachedScriptPaths_[i] == currentScript) {
						currentIndex = static_cast<int>(i);
						break;
					}
				}

				// 現在のスクリプト名を表示（パスからファイル名のみ抽出）
				std::string displayName = currentScript.empty() ? "(None)" :
					currentScript.substr(currentScript.find_last_of("/\\") + 1);

				if (ImGui::BeginCombo("Script", displayName.c_str())) {
					// Noneオプション
					if (ImGui::Selectable("(None)", currentScript.empty())) {
						luaScript->SetScriptPath("");
					}

					for (size_t i = 0; i < cachedScriptPaths_.size(); ++i) {
						// ファイル名のみ表示
						std::string scriptName = cachedScriptPaths_[i].substr(
							cachedScriptPaths_[i].find_last_of("/\\") + 1);
						bool isSelected = (currentIndex == static_cast<int>(i));

						if (ImGui::Selectable(scriptName.c_str(), isSelected)) {
							luaScript->SetScriptPath(cachedScriptPaths_[i]);
							(void)luaScript->ReloadScript();
						}

						// ツールチップでフルパス表示
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("%s", cachedScriptPaths_[i].c_str());
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				// リフレッシュボタン
				ImGui::SameLine();
				if (ImGui::Button("R##RefreshScripts")) {
					RefreshScriptPaths();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Refresh script list");
				}

				// エラー表示
				if (luaScript->HasError()) {
					auto& error = luaScript->GetLastError();
					if (error) {
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
						ImGui::TextWrapped("Error: %s", error->message.c_str());
						if (error->line >= 0) {
							ImGui::Text("Line: %d", error->line);
						}
						ImGui::PopStyleColor();
					}
				}

				// プロパティ表示
				auto properties = luaScript->GetProperties();
				if (!properties.empty()) {
					ImGui::Spacing();
					ImGui::Text("Properties:");
					ImGui::Indent();

					for (auto& prop : properties) {
						ImGui::PushID(prop.name.c_str());

						std::visit([&](auto&& val) {
							using T = std::decay_t<decltype(val)>;
							if constexpr (std::is_same_v<T, bool>) {
								bool v = val;
								if (ImGui::Checkbox(prop.name.c_str(), &v)) {
									luaScript->SetProperty(prop.name, v);
								}
							} else if constexpr (std::is_same_v<T, int32>) {
								int v = val;
								if (ImGui::DragInt(prop.name.c_str(), &v)) {
									luaScript->SetProperty(prop.name, static_cast<int32>(v));
								}
							} else if constexpr (std::is_same_v<T, float>) {
								float v = val;
								if (ImGui::DragFloat(prop.name.c_str(), &v, 0.1f)) {
									luaScript->SetProperty(prop.name, v);
								}
							} else if constexpr (std::is_same_v<T, std::string>) {
								char buffer[256];
								strncpy_s(buffer, val.c_str(), sizeof(buffer) - 1);
								if (ImGui::InputText(prop.name.c_str(), buffer, sizeof(buffer))) {
									luaScript->SetProperty(prop.name, std::string(buffer));
								}
							}
						}, prop.value);

						ImGui::PopID();
					}

					ImGui::Unindent();
				}

				// リロードボタン
				ImGui::Spacing();
				if (ImGui::Button("Reload Script")) {
					(void)luaScript->ReloadScript();
				}

				// コンポーネント削除ボタン
				ImGui::SameLine();
				if (ImGui::Button("Remove Script")) {
					selected->RemoveComponent<LuaScriptComponent>();
				}
			}

			// スクリプト追加ボタン
			if (!luaScript) {
				ImGui::Separator();
				if (ImGui::Button("Add Lua Script")) {
					selected->AddComponent<LuaScriptComponent>();
					RefreshScriptPaths();
				}
			}
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

		ImGui::Separator();
		ImGui::Text("Camera Settings");
		ImGui::Spacing();

		bool settingsChanged = false;

		// マウス感度（回転速度）
		float rotateSpeed = editorCamera_.GetRotateSpeed();
		ImGui::Text("Mouse Sensitivity");
		if (ImGui::SliderFloat("##MouseSensitivity", &rotateSpeed, 0.1f, 5.0f, "%.2f")) {
			editorCamera_.SetRotateSpeed(rotateSpeed);
			settingsChanged = true;
		}

		// 移動速度
		float moveSpeed = editorCamera_.GetMoveSpeed();
		ImGui::Text("Move Speed");
		if (ImGui::SliderFloat("##MoveSpeed", &moveSpeed, 1.0f, 100.0f, "%.1f")) {
			editorCamera_.SetMoveSpeed(moveSpeed);
			settingsChanged = true;
		}

		// スクロール速度
		float scrollSpeed = editorCamera_.GetScrollSpeed();
		ImGui::Text("Scroll Speed");
		if (ImGui::SliderFloat("##ScrollSpeed", &scrollSpeed, 0.1f, 5.0f, "%.2f")) {
			editorCamera_.SetScrollSpeed(scrollSpeed);
			settingsChanged = true;
		}

		// 設定が変更されたら保存
		if (settingsChanged) {
			editorCamera_.SaveSettings();
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
				bool isExpanded = expandedObjects_.count(obj) > 0;
				bool isRenaming = (renamingObject_ == obj);

				// ユニークIDを生成
				ImGui::PushID(static_cast<int>(i));

				// コンポーネントに応じたアイコン
				const char* icon = "📦";
				if (obj->GetComponent<CameraComponent>()) icon = "📷";  // カメラコンポーネント優先
				else if (obj->GetComponent<SkinnedMeshRenderer>()) icon = "🎭";
				else if (obj->GetComponent<DirectionalLightComponent>()) icon = "💡";
				else if (obj->GetName() == "Player") icon = "🎮";

				// 展開矢印（小さい三角形）
				bool hasTransformInfo = true;  // 全オブジェクトにTransform情報あり
				if (hasTransformInfo) {
					// 小さいボタンで三角形を表示
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

				// アイコン
				ImGui::Text("%s", icon);
				ImGui::SameLine();

				// リネームモード
				if (isRenaming) {
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::InputText("##rename", renameBuffer_, sizeof(renameBuffer_),
						ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
						// Enter押下でリネーム確定
						if (strlen(renameBuffer_) > 0) {
							obj->SetName(renameBuffer_);
							consoleMessages_.push_back("[Editor] Renamed to: " + std::string(renameBuffer_));
						}
						renamingObject_ = nullptr;
					}
					// 初回フォーカス設定
					if (ImGui::IsItemDeactivated() || (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())) {
						renamingObject_ = nullptr;
					}
					// 最初のフレームでフォーカス
					if (ImGui::IsWindowAppearing() || (renamingObject_ == obj && !ImGui::IsItemActive())) {
						ImGui::SetKeyboardFocusHere(-1);
					}
				} else {
					// 通常表示
					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
						ImGuiTreeNodeFlags_NoTreePushOnOpen |
						ImGuiTreeNodeFlags_SpanAvailWidth;
					if (selectedObject_ == obj) {
						flags |= ImGuiTreeNodeFlags_Selected;
					}

					ImGui::TreeNodeEx(obj->GetName().c_str(), flags);

					// シングルクリックで選択＋フォーカス
					if (ImGui::IsItemClicked() && !ImGui::IsMouseDoubleClicked(0)) {
						selectedObject_ = obj;
						FocusOnObject(obj);
					}

					// ダブルクリックでリネームモード開始
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
						renamingObject_ = obj;
						strncpy_s(renameBuffer_, obj->GetName().c_str(), sizeof(renameBuffer_) - 1);
						renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
					}
				}

				// AudioファイルのD&Dターゲット（オブジェクトにドロップ）
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_PATH")) {
						size_t index = *(const size_t*)payload->Data;
						if (index < cachedAudioPaths_.size()) {
							const std::string& audioPath = cachedAudioPaths_[index];
							// AudioSourceがなければ追加
							auto* audioSource = obj->GetComponent<AudioSource>();
							if (!audioSource) {
								audioSource = obj->AddComponent<AudioSource>();
								consoleMessages_.push_back("[Editor] Added AudioSource to: " + obj->GetName());
							}
							audioSource->SetClipPath(audioPath);
							audioSource->LoadClip(audioPath);
							consoleMessages_.push_back("[Audio] Set clip: " + std::filesystem::path(audioPath).filename().string());
							selectedObject_ = obj;
						}
					}
					ImGui::EndDragDropTarget();
				}

				// 右クリックメニュー
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
					// 削除不可オブジェクトはグレーアウト
					bool canDelete = obj->IsDeletable();
					if (!canDelete) {
						ImGui::BeginDisabled();
					}
					if (ImGui::MenuItem("Delete", "DEL", false, canDelete)) {
						if (gameObjects_ && canDelete) {
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
					if (!canDelete) {
						ImGui::EndDisabled();
						if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
							ImGui::SetTooltip("This object cannot be deleted");
						}
					}
					ImGui::Separator();
					if (ImGui::BeginMenu("Add Component")) {
						if (ImGui::MenuItem("AudioSource")) {
							if (!obj->GetComponent<AudioSource>()) {
								obj->AddComponent<AudioSource>();
								consoleMessages_.push_back("[Editor] Added AudioSource to: " + obj->GetName());
							}
						}
						if (ImGui::MenuItem("AudioListener")) {
							if (!obj->GetComponent<AudioListener>()) {
								obj->AddComponent<AudioListener>();
								consoleMessages_.push_back("[Editor] Added AudioListener to: " + obj->GetName());
							}
						}
						ImGui::EndMenu();
					}
					ImGui::EndPopup();
				}

				// インライン展開：Transform情報
				if (isExpanded) {
					ImGui::Indent(20.0f);

					auto& transform = obj->GetTransform();

					// ギズモ操作中は編集を無効化（競合を防ぐ）
					bool isGizmoActive = gizmoSystem_.IsUsing() && obj == selectedObject_;
					if (isGizmoActive) {
						ImGui::BeginDisabled();
					}

					// ローカル座標を使用（ギズモと統一）
					Vector3 pos = transform.GetLocalPosition();
					Quaternion rot = transform.GetLocalRotation();
					Vector3 scale = transform.GetLocalScale();

					// 回転をオイラー角に変換（Quaternion -> Euler）
					float pitch, yaw, roll;
					float qx = rot.GetX(), qy = rot.GetY(), qz = rot.GetZ(), qw = rot.GetW();

					// Roll (X軸回転)
					float sinr_cosp = 2.0f * (qw * qx + qy * qz);
					float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
					roll = std::atan2(sinr_cosp, cosr_cosp);

					// Pitch (Y軸回転)
					float sinp = 2.0f * (qw * qy - qz * qx);
					if (std::abs(sinp) >= 1.0f)
						pitch = std::copysign(3.14159265f / 2.0f, sinp);
					else
						pitch = std::asin(sinp);

					// Yaw (Z軸回転)
					float siny_cosp = 2.0f * (qw * qz + qx * qy);
					float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
					yaw = std::atan2(siny_cosp, cosy_cosp);

					float euler[3] = { roll * 57.2958f, pitch * 57.2958f, yaw * 57.2958f };

					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

					// Position（ドラッグ＆Ctrl+クリックで直接入力）
					float posArr[3] = { pos.GetX(), pos.GetY(), pos.GetZ() };
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Pos", posArr, 0.1f, 0.0f, 0.0f, "%.2f")) {
						transform.SetLocalPosition(Vector3(posArr[0], posArr[1], posArr[2]));
					}

					// Rotation（ドラッグ＆Ctrl+クリックで直接入力）
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Rot", euler, 1.0f, 0.0f, 0.0f, "%.1f")) {
						// Euler角（度）からQuaternionへ変換
						float radX = euler[0] * 0.0174533f;
						float radY = euler[1] * 0.0174533f;
						float radZ = euler[2] * 0.0174533f;
						transform.SetLocalRotation(Quaternion::RotationRollPitchYaw(radX, radY, radZ));
					}

					// Scale（ドラッグ＆Ctrl+クリックで直接入力）
					float scaleArr[3] = { scale.GetX(), scale.GetY(), scale.GetZ() };
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Scale", scaleArr, 0.01f, 0.001f, 100.0f, "%.3f")) {
						transform.SetLocalScale(Vector3(scaleArr[0], scaleArr[1], scaleArr[2]));
					}

					ImGui::PopStyleColor();

					if (isGizmoActive) {
						ImGui::EndDisabled();
					}

					// === AudioSource コンポーネント ===
					if (auto* audioSource = obj->GetComponent<AudioSource>()) {
						ImGui::Separator();
						ImGui::Text("AudioSource");
						ImGui::Indent(10.0f);

						// オーディオファイルリストをリフレッシュ（まだ空の場合）
						if (cachedAudioPaths_.empty()) {
							RefreshAudioPaths();
						}

						// クリップ選択（ドロップダウン）
						std::string currentClip = audioSource->GetClipPath();
						std::string displayName = currentClip.empty() ? "(None)" : std::filesystem::path(currentClip).filename().string();

						ImGui::SetNextItemWidth(180.0f);
						if (ImGui::BeginCombo("Audio Clip", displayName.c_str())) {
							// (None)選択肢
							if (ImGui::Selectable("(None)", currentClip.empty())) {
								audioSource->SetClipPath("");
								audioSource->SetClip(nullptr);
							}

							// assets/audioフォルダ内のファイル一覧
							for (const auto& audioPath : cachedAudioPaths_) {
								std::string filename = std::filesystem::path(audioPath).filename().string();
								bool isSelected = (currentClip == audioPath);
								if (ImGui::Selectable(filename.c_str(), isSelected)) {
									audioSource->SetClipPath(audioPath);
									audioSource->LoadClip(audioPath);
									consoleMessages_.push_back("[Audio] Loaded: " + filename);
								}
								if (isSelected) {
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}

						// D&Dターゲット（AudioClipをここにドロップ可能）
						if (ImGui::BeginDragDropTarget()) {
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_PATH")) {
								size_t index = *(const size_t*)payload->Data;
								if (index < cachedAudioPaths_.size()) {
									const std::string& audioPath = cachedAudioPaths_[index];
									audioSource->SetClipPath(audioPath);
									audioSource->LoadClip(audioPath);
									consoleMessages_.push_back("[Audio] Dropped: " + std::filesystem::path(audioPath).filename().string());
								}
							}
							ImGui::EndDragDropTarget();
						}

						ImGui::SameLine();
						if (ImGui::Button("...##AudioClip")) {
							// Win32 ファイルダイアログ（外部ファイル用）
							char filename[MAX_PATH] = "";
							OPENFILENAMEA ofn = {};
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = nullptr;
							ofn.lpstrFilter = "WAV Files\0*.wav\0All Files\0*.*\0";
							ofn.lpstrFile = filename;
							ofn.nMaxFile = MAX_PATH;
							ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
							ofn.lpstrDefExt = "wav";
							if (GetOpenFileNameA(&ofn)) {
								audioSource->SetClipPath(filename);
								audioSource->LoadClip(filename);
							}
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Browse for external WAV file");
						}

						// 音量
						float volume = audioSource->GetVolume();
						ImGui::SetNextItemWidth(150.0f);
						if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
							audioSource->SetVolume(volume);
						}

						// ループ
						bool loop = audioSource->IsLooping();
						if (ImGui::Checkbox("Loop", &loop)) {
							audioSource->SetLoop(loop);
						}

						// PlayOnAwake
						bool playOnAwake = audioSource->GetPlayOnAwake();
						if (ImGui::Checkbox("Play On Awake", &playOnAwake)) {
							audioSource->SetPlayOnAwake(playOnAwake);
						}

						// 3Dオーディオ設定
						bool is3D = audioSource->Is3D();
						if (ImGui::Checkbox("3D Audio", &is3D)) {
							audioSource->Set3D(is3D);
						}

						if (is3D) {
							float minDist = audioSource->GetMinDistance();
							float maxDist = audioSource->GetMaxDistance();
							ImGui::SetNextItemWidth(100.0f);
							if (ImGui::DragFloat("Min Distance", &minDist, 0.1f, 0.1f, 100.0f)) {
								audioSource->SetMinDistance(minDist);
							}
							ImGui::SetNextItemWidth(100.0f);
							if (ImGui::DragFloat("Max Distance", &maxDist, 1.0f, 1.0f, 1000.0f)) {
								audioSource->SetMaxDistance(maxDist);
							}
						}

						// プレビューボタン
						if (audioSource->IsPlaying()) {
							if (ImGui::Button("Stop##Audio")) {
								audioSource->Stop();
								// エディタオーバーライドをクリア
								if (AudioListener::GetInstance()) {
									AudioListener::GetInstance()->ClearEditorOverride();
								}
								previewingAudioSource_ = nullptr;
								// エディタ用リスナーを解放
								editorAudioListener_.reset();
							}
						} else {
							if (ImGui::Button("Preview##Audio")) {
								// 3Dオーディオの場合、エディタ用リスナーを作成してから再生
								if (audioSource->Is3D()) {
									// シーンにAudioListenerがない場合はエディタ用を作成
									if (!AudioListener::GetInstance()) {
										editorAudioListener_ = std::make_unique<AudioListener>();
									}
									// Scene Viewカメラ位置・向きをリスナーとして設定
									if (AudioListener::GetInstance()) {
										AudioListener::GetInstance()->SetEditorOverridePosition(
											sceneViewCamera_.GetPosition());
										AudioListener::GetInstance()->SetEditorOverrideOrientation(
											sceneViewCamera_.GetForward(),
											sceneViewCamera_.GetUp());
									}
									previewingAudioSource_ = audioSource;
								}
								audioSource->Play();
							}
						}

						// 3Dプレビュー中は現在の距離を表示
						if (audioSource->Is3D() && audioSource->IsPlaying()) {
							Vector3 sourcePos = obj->GetTransform().GetPosition();
							Vector3 camPos = sceneViewCamera_.GetPosition();
							float distance = (sourcePos - camPos).Length();
							ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
								"Distance: %.1f m", distance);
						}

						ImGui::Unindent(10.0f);
					}

					// === AudioListener コンポーネント ===
					if (obj->GetComponent<AudioListener>()) {
						ImGui::Separator();
						ImGui::Text("AudioListener");
						ImGui::TextDisabled("  (Main listener for 3D audio)");
					}

					ImGui::Unindent(20.0f);
				}

				ImGui::PopID();
			}
		}
		else {
			ImGui::TextDisabled("(no objects)");
		}

		// DELキーで選択中のオブジェクトを削除（削除不可オブジェクトは除く）
		if (selectedObject_ && !renamingObject_ && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
			if (!selectedObject_->IsDeletable()) {
				consoleMessages_.push_back("[Editor] Cannot delete: " + selectedObject_->GetName() + " (protected)");
			} else if (gameObjects_) {
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

		// F2キーでリネームモード開始
		if (selectedObject_ && !renamingObject_ && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2)) {
			renamingObject_ = selectedObject_;
			strncpy_s(renameBuffer_, selectedObject_->GetName().c_str(), sizeof(renameBuffer_) - 1);
			renameBuffer_[sizeof(renameBuffer_) - 1] = '\0';
		}

		// Escapeキーでリネームモードをキャンセル
		if (renamingObject_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			renamingObject_ = nullptr;
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

		// Audioフォルダをスキャン
		if (ImGui::TreeNode("Audio")) {
			if (ImGui::SmallButton("Refresh##Audio")) {
				RefreshAudioPaths();
				consoleMessages_.push_back("[Editor] Audio list refreshed");
			}
			ImGui::Separator();

			if (cachedAudioPaths_.empty()) {
				RefreshAudioPaths();
			}

			for (size_t i = 0; i < cachedAudioPaths_.size(); ++i) {
				const auto& audioPath = cachedAudioPaths_[i];
				std::filesystem::path p(audioPath);
				std::string filename = p.filename().string();

				ImGui::PushID(static_cast<int>(i + 10000)); // モデルとIDが被らないようにオフセット

				ImGui::Text("🔊");
				ImGui::SameLine();

				if (ImGui::Selectable(filename.c_str())) {
					// シングルクリック: AudioSourceがある選択中オブジェクトにセット
					if (selectedObject_) {
						if (auto* audioSource = selectedObject_->GetComponent<AudioSource>()) {
							audioSource->SetClipPath(audioPath);
							audioSource->LoadClip(audioPath);
							consoleMessages_.push_back("[Editor] Audio clip set: " + filename);
						}
					}
				}

				// ダブルクリック: 新規GameObjectを作成してAudioSourceを追加
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
					if (gameObjects_) {
						std::string objectName = p.stem().string(); // 拡張子なしのファイル名
						auto newObject = std::make_unique<GameObject>(objectName);
						auto* audioSource = newObject->AddComponent<AudioSource>();
						audioSource->SetClipPath(audioPath);
						audioSource->LoadClip(audioPath);
						selectedObject_ = newObject.get();
						gameObjects_->push_back(std::move(newObject));
						consoleMessages_.push_back("[Editor] Created AudioSource object: " + objectName);
					}
				}

				// ドラッグ＆ドロップソース
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
					ImGui::SetDragDropPayload("AUDIO_PATH", &i, sizeof(size_t));
					ImGui::Text("🔊 %s", filename.c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::PopID();
			}

			if (cachedAudioPaths_.empty()) {
				ImGui::TextDisabled("(no audio files found)");
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

	// オーディオパスをリフレッシュ
	void EditorUI::RefreshAudioPaths() {
		cachedAudioPaths_.clear();

		std::string audioPath = "assets/audio";
		if (std::filesystem::exists(audioPath) && std::filesystem::is_directory(audioPath)) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(audioPath)) {
				if (entry.is_regular_file()) {
					std::string ext = entry.path().extension().string();
					if (ext == ".wav" || ext == ".WAV") {
						std::string relativePath = entry.path().string();
						std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
						cachedAudioPaths_.push_back(relativePath);
					}
				}
			}
		}
	}

	void EditorUI::RefreshScriptPaths() {
		cachedScriptPaths_.clear();

		std::string scriptPath = "assets/scripts";
		if (std::filesystem::exists(scriptPath) && std::filesystem::is_directory(scriptPath)) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptPath)) {
				if (entry.is_regular_file()) {
					std::string ext = entry.path().extension().string();
					if (ext == ".lua" || ext == ".LUA") {
						std::string relativePath = entry.path().string();
						std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
						cachedScriptPaths_.push_back(relativePath);
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

			// カメラをモデルにフォーカス（新規追加なので角度もリセット）
			FocusOnNewObject(selectedObject_);

			consoleMessages_.push_back("[Editor] Created object: " + modelName);
	}

	// キューをクリア
		pendingModelLoads_.clear();
	}

	// オブジェクトにカメラをフォーカス（バウンディングボックスから距離を自動計算）
	void EditorUI::FocusOnObject(GameObject* obj) {
		if (!obj) return;

		// オブジェクトのワールド行列とスケールを取得
		auto& transform = obj->GetTransform();
		Matrix4x4 worldMatrix = transform.GetWorldMatrix();
		float m[16];
		worldMatrix.ToFloatArray(m);
		Vector3 targetPos(m[12], m[13], m[14]);
		Vector3 worldScale = transform.GetScale();

		// デフォルト距離
		float distance = 5.0f;

		// SkinnedMeshRendererがある場合はバウンディングボックスから距離を計算
		auto* renderer = obj->GetComponent<SkinnedMeshRenderer>();
		if (renderer && renderer->GetModelData()) {
			auto* modelData = renderer->GetModelData();
			if (!modelData->meshes.empty()) {
				// 全メッシュのバウンディングボックスを統合
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

				// ローカルの中心とサイズを計算
				Vector3 localCenter = (boundsMin + boundsMax) * 0.5f;
				Vector3 localSize = boundsMax - boundsMin;

				// ワールドスケールを適用
				Vector3 worldSize(
					localSize.GetX() * worldScale.GetX(),
					localSize.GetY() * worldScale.GetY(),
					localSize.GetZ() * worldScale.GetZ()
				);
				float maxDimension = (std::max)({ worldSize.GetX(), worldSize.GetY(), worldSize.GetZ() });

				// ターゲット位置をワールドスケール適用した中心に調整
				Vector3 scaledCenter(
					localCenter.GetX() * worldScale.GetX(),
					localCenter.GetY() * worldScale.GetY(),
					localCenter.GetZ() * worldScale.GetZ()
				);
				targetPos = targetPos + scaledCenter;

				// カメラ距離を計算（モデル全体が見えるように）
				distance = maxDimension * 1.5f;
				distance = (std::max)(distance, 2.0f);  // 最小距離
			}
		}

		editorCamera_.FocusOn(targetPos, distance, false);
	}

	// オブジェクトにカメラをフォーカス（新規追加時用、角度リセット）
	void EditorUI::FocusOnNewObject(GameObject* obj) {
		if (!obj) return;

		// オブジェクトのワールド行列とスケールを取得
		auto& transform = obj->GetTransform();
		Matrix4x4 worldMatrix = transform.GetWorldMatrix();
		float m[16];
		worldMatrix.ToFloatArray(m);
		Vector3 targetPos(m[12], m[13], m[14]);
		Vector3 worldScale = transform.GetScale();

		// デフォルト距離
		float distance = 5.0f;

		// SkinnedMeshRendererがある場合はバウンディングボックスから距離を計算
		auto* renderer = obj->GetComponent<SkinnedMeshRenderer>();
		if (renderer && renderer->GetModelData()) {
			auto* modelData = renderer->GetModelData();
			if (!modelData->meshes.empty()) {
				// 全メッシュのバウンディングボックスを統合
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

				// ローカルの中心とサイズを計算
				Vector3 localCenter = (boundsMin + boundsMax) * 0.5f;
				Vector3 localSize = boundsMax - boundsMin;

				// ワールドスケールを適用
				Vector3 worldSize(
					localSize.GetX() * worldScale.GetX(),
					localSize.GetY() * worldScale.GetY(),
					localSize.GetZ() * worldScale.GetZ()
				);
				float maxDimension = (std::max)({ worldSize.GetX(), worldSize.GetY(), worldSize.GetZ() });

				// ターゲット位置をワールドスケール適用した中心に調整
				Vector3 scaledCenter(
					localCenter.GetX() * worldScale.GetX(),
					localCenter.GetY() * worldScale.GetY(),
					localCenter.GetZ() * worldScale.GetZ()
				);
				targetPos = targetPos + scaledCenter;

				// カメラ距離を計算（モデル全体が見えるように）
				distance = maxDimension * 1.5f;
				distance = (std::max)(distance, 2.0f);  // 最小距離
			}
		}

		// 新規追加時は角度をリセット（斜め上から）
		editorCamera_.FocusOn(targetPos, distance, true);
	}

	void EditorUI::PrepareSceneViewGizmos(DebugRenderer* debugRenderer) {
		// DebugRendererがない場合は何もしない
		if (!debugRenderer || !gameObjects_) {
			return;
		}

		// Scene View表示中のみカメラギズモを描画
		if (!showSceneView_) {
			return;
		}

		// 全GameObjectをスキャンしてCameraComponentを持つものを探す
		for (const auto& obj : *gameObjects_) {
			auto* cameraComp = obj->GetComponent<CameraComponent>();
			if (!cameraComp) continue;

			// カメラの位置と向きを取得
			Camera* cam = cameraComp->GetCamera();
			if (!cam) continue;

			Vector3 camPos = cam->GetPosition();
			Vector3 camForward = cam->GetForward();
			Vector3 camUp = cam->GetUp();

			// カメラアイコンの色（選択中は黄色、通常は白）
			Vector4 iconColor = (selectedObject_ == obj.get())
				? Vector4(1.0f, 1.0f, 0.0f, 1.0f)  // 黄色（選択中）
				: Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白（通常）

			// カメラアイコンを描画
			float iconScale = 0.5f;
			debugRenderer->AddCameraIcon(camPos, camForward, camUp, iconScale, iconColor);

			// 選択中のカメラはFrustumも描画
			if (selectedObject_ == obj.get() || showCameraFrustum_) {
				Vector3 nearCorners[4];
				Vector3 farCorners[4];

				// 表示用に遠距離を制限（見やすさのため）
				float displayFarClip = (std::min)(cameraComp->GetFarClip(), 20.0f);
				float nearClip = cameraComp->GetNearClip();
				float fov = cameraComp->GetFieldOfView();
				float aspect = cameraComp->GetAspectRatio();

				// カメラの方向ベクトル
				Vector3 right = camUp.Cross(camForward).Normalize();

				// Frustumコーナーを直接計算（投影行列を変更せずに）
				if (cameraComp->IsOrthographic()) {
					float halfW = 5.0f;  // デフォルト幅の半分
					float halfH = 5.0f;

					Vector3 nearCenter = camPos + camForward * nearClip;
					Vector3 farCenter = camPos + camForward * displayFarClip;

					nearCorners[0] = nearCenter - right * halfW - camUp * halfH;
					nearCorners[1] = nearCenter + right * halfW - camUp * halfH;
					nearCorners[2] = nearCenter + right * halfW + camUp * halfH;
					nearCorners[3] = nearCenter - right * halfW + camUp * halfH;

					farCorners[0] = farCenter - right * halfW - camUp * halfH;
					farCorners[1] = farCenter + right * halfW - camUp * halfH;
					farCorners[2] = farCenter + right * halfW + camUp * halfH;
					farCorners[3] = farCenter - right * halfW + camUp * halfH;
				} else {
					float tanHalfFov = std::tan(fov * 0.5f);
					float nearH = nearClip * tanHalfFov;
					float nearW = nearH * aspect;
					float farH = displayFarClip * tanHalfFov;
					float farW = farH * aspect;

					Vector3 nearCenter = camPos + camForward * nearClip;
					Vector3 farCenter = camPos + camForward * displayFarClip;

					nearCorners[0] = nearCenter - right * nearW - camUp * nearH;
					nearCorners[1] = nearCenter + right * nearW - camUp * nearH;
					nearCorners[2] = nearCenter + right * nearW + camUp * nearH;
					nearCorners[3] = nearCenter - right * nearW + camUp * nearH;

					farCorners[0] = farCenter - right * farW - camUp * farH;
					farCorners[1] = farCenter + right * farW - camUp * farH;
					farCorners[2] = farCenter + right * farW + camUp * farH;
					farCorners[3] = farCenter - right * farW + camUp * farH;
				}

				// Frustumを描画（半透明の青）
				Vector4 frustumColor(0.3f, 0.6f, 1.0f, 1.0f);
				debugRenderer->AddCameraFrustum(nearCorners, farCorners, frustumColor);
			}
		}
	}

} // namespace UnoEngine
