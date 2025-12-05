#include "pch.h"
#include "EditorUI.h"
#include "../../Engine/Graphics/GraphicsDevice.h"
#include "../../Engine/Rendering/DebugRenderer.h"
#include "../../Engine/Animation/AnimationSystem.h"
#include "../../Engine/Scene/SceneSerializer.h"
#include "../../Engine/Rendering/SkinnedMeshRenderer.h"
#include "../../Engine/Animation/AnimatorComponent.h"
#include "../../Engine/Resource/ResourceManager.h"
#include "../../Engine/Resource/SkinnedModelImporter.h"
#include "../../Engine/Resource/StaticModelImporter.h"
#include "../../Engine/Graphics/MeshRenderer.h"
#include "../../Engine/Graphics/SkinnedVertex.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Math/BoundingVolume.h"
#include "../../Engine/Audio/AudioSystem.h"
#include "../../Engine/Audio/AudioSource.h"
#include "../../Engine/Audio/AudioListener.h"
#include "../../Engine/Audio/AudioClip.h"
#include "../../Engine/Core/CameraComponent.h"
#include "../../Engine/Editor/ParticleEditor.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "../../Engine/UI/imgui_toggle.h"
#include "../../Engine/UI/imgui_toggle_presets.h"
#include "ImGuizmo.h"
#include <algorithm>
#include <cmath>

// C++20 u8リテラルをconst char*に変換するヘルパー
#define U8(str) reinterpret_cast<const char*>(u8##str)

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

		// ポストプロセス出力用テクスチャ (SRVインデックス 5)
		postProcessOutput_.Create(graphics, 1280, 720, 5);

		// ポストプロセスマネージャー初期化
		postProcessManager_ = std::make_unique<PostProcessManager>();
		postProcessManager_->Initialize(graphics, 1280, 720);

		// Scene View用カメラの初期化（Main Cameraとは完全に独立）
		sceneViewCamera_.SetPerspective(
			60.0f * 0.0174533f,  // FOV 60度
			16.0f / 9.0f,        // アスペクト比
			0.1f,                // Near clip
			1000.0f              // Far clip
		);
		sceneViewCamera_.SetPosition(Vector3(0.0f, 5.0f, -10.0f));
		// 少し下を向く（原点を見る感じ）
		sceneViewCamera_.SetRotation(Quaternion::RotationRollPitchYaw(0.3f, 0.0f, 0.0f));
		// ビュー行列を即座に更新（最初のフレームで正しい状態にする）
		sceneViewCamera_.GetViewMatrix();

		// EditorCameraにScene View用カメラを設定
		editorCamera_.SetCamera(&sceneViewCamera_);

		// ギズモシステム初期化
		gizmoSystem_.Initialize();

		// エディタカメラ設定を読み込み
		editorCamera_.LoadSettings();

		// Console初期ログ
		consoleMessages_.push_back(U8("[システム] UnoEngine エディタを初期化しました"));
		consoleMessages_.push_back(U8("[情報] ~ キーでコンソール切り替え"));
		consoleMessages_.push_back(U8("[情報] Q: 移動, E: 回転, R: スケール"));
	}

	void EditorUI::Render(const EditorContext& context) {
		// ImGuizmoフレーム開始
		ImGuizmo::BeginFrame();

		// スクリプトファイル監視（ホットリロード）
		UpdateScriptFileWatcher();

		// Game Camera（Main Camera）を設定（未設定の場合のみ）
		if (!gameCamera_ && context.camera) {
			gameCamera_ = context.camera;
		}

		// Scene Viewのアスペクト比を更新（変更があった場合のみ）
		if (desiredSceneViewWidth_ > 0 && desiredSceneViewHeight_ > 0) {
			float newAspect = static_cast<float>(desiredSceneViewWidth_) / static_cast<float>(desiredSceneViewHeight_);
			float currentAspect = sceneViewCamera_.GetAspectRatio();
			// アスペクト比が変わった場合のみ更新（浮動小数点の誤差を考慮）
			if (std::abs(newAspect - currentAspect) > 0.001f) {
				sceneViewCamera_.SetPerspective(
					60.0f * 0.0174533f,  // FOV 60度
					newAspect,
					0.1f,
					1000.0f
				);
			}
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
		RenderWorldOutliner(context);   // 新: 左側パネル（World Outliner & Assets）
		RenderObjectProperties(context); // 新: 右側パネル（Object Properties）
		RenderConsoleAndDebugger();      // 新: 下部パネル（Console & Debugger）
		RenderHierarchy(context);        // 互換性のため残す
		RenderInspector(context);        // 互換性のため残す
		RenderStats(context);            // Stats（右側に統合予定）
		RenderConsole();                 // 互換性のため残す
		RenderProject(context);
		RenderProfiler();

		// パーティクルエディター描画
		if (particleEditor_) {
			particleEditor_->Draw();
		}

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
			if (ImGui::BeginMenu(U8("表示"))) {
				ImGui::SeparatorText(U8("ビューポート"));
				ImGui::MenuItem(U8("シーンビュー"), "F1", &showSceneView_);
				ImGui::MenuItem(U8("ゲームビュー"), "F2", &showGameView_);

				ImGui::SeparatorText(U8("ツール"));
				ImGui::MenuItem(U8("インスペクター"), nullptr, &showInspector_);
				ImGui::MenuItem(U8("ヒエラルキー"), nullptr, &showHierarchy_);
				ImGui::MenuItem(U8("コンソール"), nullptr, &showConsole_);
				ImGui::MenuItem(U8("プロジェクト"), nullptr, &showProject_);

				ImGui::SeparatorText(U8("パフォーマンス"));
				ImGui::MenuItem(U8("統計情報"), nullptr, &showStats_);
				ImGui::MenuItem(U8("プロファイラー"), nullptr, &showProfiler_);

				ImGui::SeparatorText(U8("エフェクト"));
				if (particleEditor_) {
					bool particleEditorVisible = particleEditor_->IsVisible();
					if (ImGui::MenuItem(U8("パーティクルエディタ"), nullptr, &particleEditorVisible)) {
						particleEditor_->SetVisible(particleEditorVisible);
					}
				} else {
					ImGui::MenuItem(U8("パーティクルエディタ (利用不可)"), nullptr, false, false);
				}

				ImGui::Separator();
				if (ImGui::MenuItem(U8("レイアウトをリセット"), "Ctrl+Shift+R")) {
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
			const char* modeText = U8("編集中");
			ImVec4 modeColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
			if (isPlaying) {
				modeText = U8("再生中");
				modeColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
			}
			else if (isPaused) {
				modeText = U8("一時停止");
				modeColor = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
			}
			ImGui::TextColored(modeColor, "%s", modeText);

			ImGui::EndMenuBar();
		}

		// 初回起動時にデフォルトレイアウトを構築（Unity風レイアウト）
		if (!dockingLayoutInitialized_) {
			dockingLayoutInitialized_ = true;

			// レイアウトをリセット
			ImGui::DockBuilderRemoveNode(dockspaceID);
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

			// ドックスペースを分割（Unity風: 左-中央-右、下）
			ImGuiID dock_main, dock_bottom;
			ImGuiID dock_left, dock_center_right;
			ImGuiID dock_center, dock_right;
			ImGuiID dock_project, dock_console;

			// メイン領域(70%) | 下部(30%)
			dock_main = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Up, 0.70f, nullptr, &dock_bottom);

			// メイン領域を左(15%) | 中央+右(85%)に分割
			dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.15f, nullptr, &dock_center_right);

			// 中央+右を中央(75%) | 右(25%)に分割
			dock_center = ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Left, 0.75f, nullptr, &dock_right);

			// 下部を左(15%) | 右(85%)に分割
			dock_project = ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Left, 0.15f, nullptr, &dock_console);

			// パネルをドックに配置
			// 左: ワールドアウトライナー & アセット
			ImGui::DockBuilderDockWindow(U8("ワールドアウトライナー"), dock_left);

			// 中央: シーンビュー / ゲームビュー
			ImGui::DockBuilderDockWindow(U8("シーン"), dock_center);
			ImGui::DockBuilderDockWindow(U8("ゲーム"), dock_center);

			// 右: オブジェクトプロパティ
			ImGui::DockBuilderDockWindow(U8("プロパティ"), dock_right);

			// 下部左: プロジェクト
			ImGui::DockBuilderDockWindow(U8("プロジェクト"), dock_project);

			// 下部右: コンソール & デバッガ
			ImGui::DockBuilderDockWindow(U8("コンソール"), dock_console);

			// 旧ウィンドウ名も配置（互換性）
			ImGui::DockBuilderDockWindow(U8("ヒエラルキー"), dock_left);
			ImGui::DockBuilderDockWindow(U8("インスペクター"), dock_right);
			ImGui::DockBuilderDockWindow(U8("統計情報"), dock_right);
			ImGui::DockBuilderDockWindow(U8("プロファイラー"), dock_console);

			ImGui::DockBuilderFinish(dockspaceID);
		}

		ImGui::End();
	}

	void EditorUI::RenderSceneView() {
		if (!showSceneView_) return;

		ImGui::Begin(U8("シーン"), &showSceneView_);

		// ========================================
		// Scene View ツールバー（Unity風）
		// ========================================
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

		// 描画モード選択
		static int shadingMode = 0;
		const char* shadingModes[] = { U8("シェーディング"), U8("ワイヤーフレーム"), U8("両方") };
		ImGui::SetNextItemWidth(120.0f);
		ImGui::Combo("##Shading", &shadingMode, shadingModes, IM_ARRAYSIZE(shadingModes));
		ImGui::SameLine();

		// 2Dボタン
		static bool is2DMode = false;
		if (ImGui::Button(is2DMode ? "3D" : "2D", ImVec2(30, 0))) {
			is2DMode = !is2DMode;
		}
		ImGui::SameLine();

		// ギズモツールボタン
		ImGui::Separator();
		ImGui::SameLine();

		// Move Tool
		bool isTranslate = (gizmoSystem_.GetOperation() == GizmoOperation::Translate);
		if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		if (ImGui::Button(U8("移動"), ImVec2(40, 0))) {
			gizmoSystem_.SetOperation(GizmoOperation::Translate);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("移動ツール (Q)"));
		if (isTranslate) ImGui::PopStyleColor();
		ImGui::SameLine();

		// Rotate Tool
		bool isRotate = (gizmoSystem_.GetOperation() == GizmoOperation::Rotate);
		if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		if (ImGui::Button(U8("回転"), ImVec2(40, 0))) {
			gizmoSystem_.SetOperation(GizmoOperation::Rotate);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("回転ツール (W)"));
		if (isRotate) ImGui::PopStyleColor();
		ImGui::SameLine();

		// Scale Tool
		bool isScale = (gizmoSystem_.GetOperation() == GizmoOperation::Scale);
		if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		if (ImGui::Button(U8("拡縮"), ImVec2(40, 0))) {
			gizmoSystem_.SetOperation(GizmoOperation::Scale);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("スケールツール (E)"));
		if (isScale) ImGui::PopStyleColor();
		ImGui::SameLine();

		ImGui::Separator();
		ImGui::SameLine();

		// 座標系切り替え（Local/Global）
		static bool isLocalSpace = true;
		if (ImGui::Button(isLocalSpace ? U8("ローカル") : U8("グローバル"), ImVec2(70, 0))) {
			isLocalSpace = !isLocalSpace;
		}
		ImGui::SameLine();

		// Pivot/Center
		static bool isPivot = true;
		if (ImGui::Button(isPivot ? U8("ピボット") : U8("中心"), ImVec2(60, 0))) {
			isPivot = !isPivot;
		}
		ImGui::SameLine();

		ImGui::Separator();
		ImGui::SameLine();

		// Gizmosドロップダウン
		if (ImGui::Button(U8("ギズモ"))) {
			ImGui::OpenPopup("GizmosPopup");
		}
		if (ImGui::BeginPopup("GizmosPopup")) {
			ImGui::Checkbox(U8("グリッド表示"), &showCameraFrustum_);
			ImGui::Checkbox(U8("カメラ視錐台"), &showCameraFrustum_);
			ImGui::EndPopup();
		}
		ImGui::SameLine();

		// 右寄せで表示オプション
		float windowWidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + windowWidth - 100);
		ImGui::Text(U8("全て"));

		ImGui::PopStyleVar(2);
		ImGui::Separator();

		// ========================================
		// Scene View 本体
		// ========================================

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

			// SceneViewでのクリック選択処理
			HandleSceneViewPicking();

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

				// ギズモで回転操作された場合、キャッシュをクリア（再計算させる）
				if (manipulated && gizmoSystem_.GetOperation() == GizmoOperation::Rotate) {
					uint64_t objId = reinterpret_cast<uint64_t>(selectedObject_);
					cachedEulerAngles_.erase(objId);
				}

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

		ImGui::Begin(U8("ゲーム"), &showGameView_);

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

			// ポストプロセスが有効な場合は出力テクスチャを表示
			D3D12_GPU_DESCRIPTOR_HANDLE displayHandle = gameViewTexture_.GetSRVHandle();
			if (postProcessManager_ && postProcessManager_->GetActiveEffect() != PostProcessType::None) {
				displayHandle = postProcessOutput_.GetSRVHandle();
			}
			ImGui::Image((ImTextureID)displayHandle.ptr, imageSize);

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

		ImGui::Begin(U8("インスペクター"), &showInspector_);

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
						isDirty_ = true;
					}

					for (size_t i = 0; i < cachedScriptPaths_.size(); ++i) {
						// ファイル名のみ表示
						std::string scriptName = cachedScriptPaths_[i].substr(
							cachedScriptPaths_[i].find_last_of("/\\") + 1);
						bool isSelected = (currentIndex == static_cast<int>(i));

						if (ImGui::Selectable(scriptName.c_str(), isSelected)) {
							luaScript->SetScriptPath(cachedScriptPaths_[i]);
							(void)luaScript->ReloadScript();
							isDirty_ = true;
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
								isDirty_ = true;
							}
						} else if constexpr (std::is_same_v<T, int32>) {
								int v = val;
								if (ImGui::DragInt(prop.name.c_str(), &v)) {
									luaScript->SetProperty(prop.name, static_cast<int32>(v));
								isDirty_ = true;
							}
						} else if constexpr (std::is_same_v<T, float>) {
								float v = val;
								if (ImGui::DragFloat(prop.name.c_str(), &v, 0.1f)) {
									luaScript->SetProperty(prop.name, v);
								isDirty_ = true;
							}
						} else if constexpr (std::is_same_v<T, std::string>) {
								char buffer[256];
								strncpy_s(buffer, val.c_str(), sizeof(buffer) - 1);
								if (ImGui::InputText(prop.name.c_str(), buffer, sizeof(buffer))) {
									luaScript->SetProperty(prop.name, std::string(buffer));
								isDirty_ = true;
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
							isDirty_ = true;
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

	// ============================================================
	// 新しいUnity風パネル
	// ============================================================

	void EditorUI::RenderWorldOutliner(const EditorContext& context) {
		ImGui::Begin(U8("ワールドアウトライナー"));

		// タブバー（ワールドアウトライナー / アセット）
		if (ImGui::BeginTabBar("WorldOutlinerTabs")) {
			// ワールドアウトライナー タブ
			if (ImGui::BeginTabItem(U8("アウトライナー"))) {
				ImGui::Spacing();

				// 選択解除ボタン
				if (selectedObject_ && ImGui::SmallButton(U8("選択解除"))) {
					selectedObject_ = nullptr;
				}
				ImGui::SameLine();
				if (context.gameObjects) {
					ImGui::TextDisabled(U8("(%zu オブジェクト)"), context.gameObjects->size());
				}
				ImGui::Separator();

				// オブジェクトリスト
				if (context.gameObjects) {
					for (size_t i = 0; i < context.gameObjects->size(); ++i) {
						GameObject* obj = (*context.gameObjects)[i].get();

						ImGui::PushID(static_cast<int>(i));

						// アイコン
						const char* icon = "  ";
						if (obj->GetComponent<CameraComponent>()) icon = "  ";
						else if (obj->GetComponent<SkinnedMeshRenderer>()) icon = "  ";
						else if (obj->GetComponent<DirectionalLightComponent>()) icon = "  ";

						// 展開矢印
						bool hasChildren = false; // 将来の親子関係対応用
						if (hasChildren) {
							ImGui::Text(">");
						} else {
							ImGui::Text(" ");
						}
						ImGui::SameLine();

						// オブジェクト名
						ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
							ImGuiTreeNodeFlags_NoTreePushOnOpen |
							ImGuiTreeNodeFlags_SpanAvailWidth;
						if (selectedObject_ == obj) {
							flags |= ImGuiTreeNodeFlags_Selected;
						}

						ImGui::TreeNodeEx(obj->GetName().c_str(), flags);

						// クリックで選択
						if (ImGui::IsItemClicked()) {
							selectedObject_ = obj;
							FocusOnObject(obj);
						}

						// 右クリックメニュー
						if (ImGui::BeginPopupContextItem()) {
							if (ImGui::MenuItem(U8("フォーカス"), "F")) {
								FocusOnObject(obj);
							}
							if (ImGui::MenuItem(U8("名前変更"), "F2")) {
								renamingObject_ = obj;
								strncpy_s(renameBuffer_, obj->GetName().c_str(), sizeof(renameBuffer_) - 1);
							}
							ImGui::Separator();
							bool canDelete = obj->IsDeletable();
							if (!canDelete) ImGui::BeginDisabled();
							if (ImGui::MenuItem(U8("削除"), "DEL", false, canDelete)) {
								if (gameObjects_) {
									for (auto it = gameObjects_->begin(); it != gameObjects_->end(); ++it) {
										if (it->get() == obj) {
											consoleMessages_.push_back(U8("[エディタ] 削除: ") + obj->GetName());
									gameObjects_->erase(it);
									if (selectedObject_ == obj) selectedObject_ = nullptr;
									isDirty_ = true;
									break;
										}
									}
								}
							}
							if (!canDelete) ImGui::EndDisabled();
							ImGui::EndPopup();
						}

						ImGui::PopID();
					}
				} else {
					ImGui::TextDisabled("(no objects)");
				}

				ImGui::EndTabItem();
			}

			// アセット タブ（Projectからの簡易版）
			if (ImGui::BeginTabItem(U8("アセット"))) {
				ImGui::Spacing();

				// アセットリスト
				if (ImGui::CollapsingHeader(U8("モデル"), ImGuiTreeNodeFlags_DefaultOpen)) {
					if (cachedModelPaths_.empty()) {
						RefreshModelPaths();
					}
					for (size_t i = 0; i < cachedModelPaths_.size(); ++i) {
						std::filesystem::path p(cachedModelPaths_[i]);
						std::string ext = p.extension().string();
						if (ext == ".obj") continue;  // OBJはスキップ

						ImGui::PushID(static_cast<int>(i));
						if (ImGui::Selectable(p.filename().string().c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
							if (ImGui::IsMouseDoubleClicked(0)) {
								HandleModelDragDropByIndex(i);
							}
						}
						// ドラッグソース
						if (ImGui::BeginDragDropSource()) {
							ImGui::SetDragDropPayload("MODEL_INDEX", &i, sizeof(size_t));
							ImGui::Text("  %s", p.filename().string().c_str());
							ImGui::EndDragDropSource();
						}
						ImGui::PopID();
					}
				}

				if (ImGui::CollapsingHeader(U8("オーディオ"))) {
					if (cachedAudioPaths_.empty()) {
						RefreshAudioPaths();
					}
					for (size_t i = 0; i < cachedAudioPaths_.size(); ++i) {
						std::filesystem::path p(cachedAudioPaths_[i]);
						ImGui::PushID(static_cast<int>(i + 10000));
						if (ImGui::Selectable(p.filename().string().c_str())) {
							if (selectedObject_) {
								if (auto* audioSource = selectedObject_->GetComponent<AudioSource>()) {
									audioSource->SetClipPath(cachedAudioPaths_[i]);
									audioSource->LoadClip(cachedAudioPaths_[i]);
								}
							}
						}
						// ドラッグソース
						if (ImGui::BeginDragDropSource()) {
							ImGui::SetDragDropPayload("AUDIO_PATH", &i, sizeof(size_t));
							ImGui::Text("  %s", p.filename().string().c_str());
							ImGui::EndDragDropSource();
						}
						ImGui::PopID();
					}
				}

				if (ImGui::CollapsingHeader(U8("スクリプト"))) {
					if (cachedScriptPaths_.empty()) {
						RefreshScriptPaths();
					}
					for (size_t i = 0; i < cachedScriptPaths_.size(); ++i) {
						std::filesystem::path p(cachedScriptPaths_[i]);
						ImGui::PushID(static_cast<int>(i + 20000));
						if (ImGui::Selectable(p.filename().string().c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
							if (ImGui::IsMouseDoubleClicked(0)) {
								OpenScriptInVSCode(cachedScriptPaths_[i]);
							}
						}
						ImGui::PopID();
					}
				}

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	void EditorUI::RenderObjectProperties(const EditorContext& context) {
		ImGui::Begin(U8("プロパティ"));

		GameObject* selected = selectedObject_ ? selectedObject_ : context.player;

		if (selected) {
			// ヘッダー（オブジェクト名）
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			bool isActive = selected->IsActive();
			if (ImGui::Checkbox("##active", &isActive)) {
				selected->SetActive(isActive);
			}
			ImGui::SameLine();
			ImGui::Text("%s", selected->GetName().c_str());
			ImGui::PopStyleColor();

			// タグ・レイヤー（将来実装用）
			ImGui::Text(U8("タグ"));
			ImGui::SameLine(80.0f);
			if (ImGui::BeginCombo("##Tag", "MainCamera", ImGuiComboFlags_NoArrowButton)) {
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::Text(U8("レイヤー"));
			ImGui::SameLine();
			if (ImGui::BeginCombo("##Layer", "Default", ImGuiComboFlags_NoArrowButton)) {
				ImGui::EndCombo();
			}

			ImGui::Separator();

			// Transform セクション
			if (ImGui::CollapsingHeader(U8("トランスフォーム"), ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& transform = selected->GetTransform();
				Vector3 pos = transform.GetLocalPosition();
				Vector3 scale = transform.GetLocalScale();

				// Position
				float posArr[3] = { pos.GetX(), pos.GetY(), pos.GetZ() };
				ImGui::Text(U8("位置"));
				ImGui::SameLine(80.0f);
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Position", posArr, 0.1f)) {
					transform.SetLocalPosition(Vector3(posArr[0], posArr[1], posArr[2]));
				}
				if (ImGui::IsItemActivated()) BeginInspectorEdit(selected);
				if (ImGui::IsItemDeactivatedAfterEdit()) EndInspectorEdit();

				// Rotation（オイラー角）
				uint64_t objId = reinterpret_cast<uint64_t>(selected);
				if (cachedEulerAngles_.find(objId) == cachedEulerAngles_.end()) {
					Quaternion rot = transform.GetLocalRotation().Normalize();
					float qx = rot.GetX(), qy = rot.GetY(), qz = rot.GetZ(), qw = rot.GetW();
					float sinX = 2.0f * (qw * qx - qy * qz);
					float cosX = 1.0f - 2.0f * (qx * qx + qz * qz);
					float xRad = std::atan2(sinX, cosX);
					float sinY = std::clamp(2.0f * (qw * qy + qx * qz), -1.0f, 1.0f);
					float yRad = std::asin(sinY);
					float sinZ = 2.0f * (qw * qz - qx * qy);
					float cosZ = 1.0f - 2.0f * (qy * qy + qz * qz);
					float zRad = std::atan2(sinZ, cosZ);
					constexpr float RAD_TO_DEG = 57.2957795f;
					cachedEulerAngles_[objId] = Vector3(xRad * RAD_TO_DEG, yRad * RAD_TO_DEG, zRad * RAD_TO_DEG);
				}
				Vector3& cachedEuler = cachedEulerAngles_[objId];
				float euler[3] = { cachedEuler.GetX(), cachedEuler.GetY(), cachedEuler.GetZ() };

				ImGui::Text(U8("回転"));
				ImGui::SameLine(80.0f);
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Rotation", euler, 1.0f)) {
					cachedEuler = Vector3(euler[0], euler[1], euler[2]);
					constexpr float DEG_TO_RAD = 0.0174532925f;
					transform.SetLocalRotation(Quaternion::RotationRollPitchYaw(
						euler[0] * DEG_TO_RAD, euler[1] * DEG_TO_RAD, euler[2] * DEG_TO_RAD));
				}
				if (ImGui::IsItemActivated()) BeginInspectorEdit(selected);
				if (ImGui::IsItemDeactivatedAfterEdit()) EndInspectorEdit();

				// Scale
				float scaleArr[3] = { scale.GetX(), scale.GetY(), scale.GetZ() };
				ImGui::Text(U8("スケール"));
				ImGui::SameLine(80.0f);
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Scale", scaleArr, 0.01f, 0.001f, 100.0f)) {
					transform.SetLocalScale(Vector3(scaleArr[0], scaleArr[1], scaleArr[2]));
				}
				if (ImGui::IsItemActivated()) BeginInspectorEdit(selected);
				if (ImGui::IsItemDeactivatedAfterEdit()) EndInspectorEdit();
			}

			// Physics セクション（将来実装用）
			if (ImGui::CollapsingHeader(U8("物理"))) {
				ImGui::TextDisabled(U8("(物理コンポーネントなし)"));
			}

			// Camera セクション
			if (auto* camComp = selected->GetComponent<CameraComponent>()) {
				if (ImGui::CollapsingHeader(U8("カメラ"), ImGuiTreeNodeFlags_DefaultOpen)) {
					// Clear Flags
					ImGui::Text(U8("クリアフラグ"));
					ImGui::SameLine(100.0f);
					if (ImGui::BeginCombo("##ClearFlags", U8("スカイボックス"))) {
						ImGui::Selectable(U8("スカイボックス"));
						ImGui::Selectable(U8("単色"));
						ImGui::Selectable(U8("深度のみ"));
						ImGui::EndCombo();
					}

					// Projection
					bool isOrtho = camComp->IsOrthographic();
					ImGui::Text(U8("投影"));
					ImGui::SameLine(100.0f);
					if (ImGui::BeginCombo("##Projection", isOrtho ? U8("正投影") : U8("透視投影"))) {
						if (ImGui::Selectable(U8("透視投影"), !isOrtho)) {
							// 切り替え処理
						}
						if (ImGui::Selectable(U8("正投影"), isOrtho)) {
							// 切り替え処理
						}
						ImGui::EndCombo();
					}

					// FOV
					float fov = camComp->GetFieldOfView() * 57.2957795f;
					ImGui::Text(U8("視野角"));
					ImGui::SameLine(100.0f);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::SliderFloat("##FOV", &fov, 1.0f, 179.0f)) {
						camComp->SetFieldOfView(fov * 0.0174533f);
						isDirty_ = true;
					}

					// Clipping Planes
					float nearClip = camComp->GetNearClip();
					float farClip = camComp->GetFarClip();
					ImGui::Text(U8("クリッピング面"));
					ImGui::Indent(20.0f);
					ImGui::Text(U8("近"));
					ImGui::SameLine(60.0f);
					ImGui::SetNextItemWidth(100.0f);
					if (ImGui::DragFloat("##Near", &nearClip, 0.01f, 0.01f, 10.0f)) {
						camComp->SetNearClip(nearClip);
						isDirty_ = true;
					}
					ImGui::Text(U8("遠"));
					ImGui::SameLine(60.0f);
					ImGui::SetNextItemWidth(100.0f);
					if (ImGui::DragFloat("##Far", &farClip, 1.0f, 10.0f, 10000.0f)) {
						camComp->SetFarClip(farClip);
						isDirty_ = true;
					}
					ImGui::Unindent(20.0f);

					// Viewport Rect
					ImGui::Text(U8("ビューポート矩形"));
					ImGui::Indent(20.0f);
					float vpRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
					ImGui::Text("X");
					ImGui::SameLine(30.0f);
					ImGui::SetNextItemWidth(60.0f);
					ImGui::DragFloat("##VPX", &vpRect[0], 0.01f, 0.0f, 1.0f);
					ImGui::SameLine();
					ImGui::Text("Y");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(60.0f);
					ImGui::DragFloat("##VPY", &vpRect[1], 0.01f, 0.0f, 1.0f);
					ImGui::Text("W");
					ImGui::SameLine(30.0f);
					ImGui::SetNextItemWidth(60.0f);
					ImGui::DragFloat("##VPW", &vpRect[2], 0.01f, 0.0f, 1.0f);
					ImGui::SameLine();
					ImGui::Text("H");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(60.0f);
					ImGui::DragFloat("##VPH", &vpRect[3], 0.01f, 0.0f, 1.0f);
					ImGui::Unindent(20.0f);

					// Depth
					int depth = -1;
					ImGui::Text(U8("深度"));
					ImGui::SameLine(100.0f);
					ImGui::SetNextItemWidth(-1);
					ImGui::DragInt("##Depth", &depth);

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();

					// Post Processing
					ImGui::Text(U8("ポストプロセス"));
					ImGui::Indent(20.0f);

					bool ppEnabled = camComp->IsPostProcessEnabled();
					if (ImGui::Checkbox(U8("有効##PostProcess"), &ppEnabled)) {
						camComp->SetPostProcessEnabled(ppEnabled);
						isDirty_ = true;
					}

					if (ppEnabled) {
						// 複数エフェクト選択UI
						ImGui::Text(U8("エフェクトチェーン"));
						ImGui::SameLine(120.0f);
						ImGui::TextDisabled("(?)");
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip(U8("チェックしたエフェクトが上から順に適用されます"));
						}

						for (int i = 1; i < static_cast<int>(PostProcessType::Count); ++i) {
							auto effectType = static_cast<PostProcessType>(i);
							bool hasEffect = camComp->HasPostProcessEffect(effectType);
							if (ImGui::Checkbox(PostProcessManager::GetEffectName(i), &hasEffect)) {
								if (hasEffect) {
									camComp->AddPostProcessEffect(effectType);
								} else {
									camComp->RemovePostProcessEffect(effectType);
								}
								isDirty_ = true;
							}

							// エフェクトが有効な場合、そのパラメータを表示
							if (hasEffect) {
								ImGui::Indent(20.0f);
								if (effectType == PostProcessType::Grayscale) {
									if (auto* grayscale = postProcessManager_->GetGrayscale()) {
										auto& params = grayscale->GetParams();
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##GrayIntensity", &params.intensity, 0.0f, 1.0f, U8("強度: %.2f"))) {
											camComp->SetGrayscaleParams(params);
											isDirty_ = true;
										}
									}
								}
								else if (effectType == PostProcessType::Vignette) {
									if (auto* vignette = postProcessManager_->GetVignette()) {
										auto& params = vignette->GetParams();
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##VigRadius", &params.radius, 0.1f, 1.5f, U8("半径: %.2f"))) {
											camComp->SetVignetteParams(params);
											isDirty_ = true;
										}
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##VigSoftness", &params.softness, 0.01f, 1.0f, U8("柔らかさ: %.2f"))) {
											camComp->SetVignetteParams(params);
											isDirty_ = true;
										}
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##VigIntensity", &params.intensity, 0.0f, 1.0f, U8("強度: %.2f"))) {
											camComp->SetVignetteParams(params);
											isDirty_ = true;
										}
									}
								}
								else if (effectType == PostProcessType::Fisheye) {
									if (auto* fisheye = postProcessManager_->GetFisheye()) {
										auto& params = fisheye->GetParams();
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##FishStrength", &params.strength, -1.0f, 1.0f, U8("歪み: %.2f"))) {
											camComp->SetFisheyeParams(params);
											isDirty_ = true;
										}
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##FishZoom", &params.zoom, 0.5f, 2.0f, U8("ズーム: %.2f"))) {
											camComp->SetFisheyeParams(params);
											isDirty_ = true;
										}
									}
								}
								ImGui::Unindent(20.0f);
							}
						}
					}

					ImGui::Unindent(20.0f);
				}
			}

			// Audio Listener セクション
			if (selected->GetComponent<AudioListener>()) {
				if (ImGui::CollapsingHeader(U8("オーディオリスナー"), ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::TextDisabled(U8("(3Dオーディオのメインリスナー)"));
				}
			}

			// Audio Source セクション
			if (auto* audioSource = selected->GetComponent<AudioSource>()) {
				if (ImGui::CollapsingHeader(U8("オーディオソース"), ImGuiTreeNodeFlags_DefaultOpen)) {
					// クリップ選択
					std::string clipName = audioSource->GetClipPath().empty() ? U8("(なし)") :
						std::filesystem::path(audioSource->GetClipPath()).filename().string();
					ImGui::Text(U8("オーディオクリップ"));
					ImGui::SameLine(120.0f);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::BeginCombo("##AudioClip", clipName.c_str())) {
						if (ImGui::Selectable(U8("(なし)"), audioSource->GetClipPath().empty())) {
							audioSource->SetClipPath("");
						audioSource->SetClip(nullptr);
						isDirty_ = true;
						}
						for (const auto& path : cachedAudioPaths_) {
							std::string filename = std::filesystem::path(path).filename().string();
							if (ImGui::Selectable(filename.c_str(), audioSource->GetClipPath() == path)) {
								audioSource->SetClipPath(path);
								audioSource->LoadClip(path);
							isDirty_ = true;
							}
						}
						ImGui::EndCombo();
					}

					// ボリューム
					float volume = audioSource->GetVolume();
					ImGui::Text(U8("音量"));
					ImGui::SameLine(120.0f);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::SliderFloat("##Volume", &volume, 0.0f, 1.0f)) {
						audioSource->SetVolume(volume);
						isDirty_ = true;
					}

					// ループ
					bool loop = audioSource->IsLooping();
					ImGui::Text(U8("ループ"));
					ImGui::SameLine(100.0f);
					if (ImGui::Checkbox("##Loop", &loop)) {
						audioSource->SetLoop(loop);
						isDirty_ = true;
					}

					// 開始時再生
					bool playOnAwake = audioSource->GetPlayOnAwake();
					ImGui::SameLine();
					ImGui::Text(U8("開始時再生"));
					ImGui::SameLine();
					if (ImGui::Checkbox("##PlayOnAwake", &playOnAwake)) {
						audioSource->SetPlayOnAwake(playOnAwake);
						isDirty_ = true;
					}

					// プレビュー
					ImGui::Spacing();
					if (audioSource->IsPlaying()) {
						if (ImGui::Button(U8("停止"))) {
							audioSource->Stop();
						}
					} else {
						if (ImGui::Button(U8("プレビュー"))) {
							audioSource->Play();
						}
					}
				}
			}

			// Scripts セクション
			if (auto* luaScript = selected->GetComponent<LuaScriptComponent>()) {
				if (ImGui::CollapsingHeader(U8("スクリプト"), ImGuiTreeNodeFlags_DefaultOpen)) {
					std::string scriptName = luaScript->GetScriptPath().empty() ? U8("(なし)") :
						std::filesystem::path(luaScript->GetScriptPath()).filename().string();
					ImGui::Text(U8("スクリプト"));
					ImGui::SameLine(100.0f);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::BeginCombo("##Script", scriptName.c_str())) {
						if (ImGui::Selectable(U8("(なし)"), luaScript->GetScriptPath().empty())) {
							luaScript->SetScriptPath("");
						isDirty_ = true;
						}
						for (const auto& path : cachedScriptPaths_) {
							std::string filename = std::filesystem::path(path).filename().string();
							if (ImGui::Selectable(filename.c_str(), luaScript->GetScriptPath() == path)) {
								luaScript->SetScriptPath(path);
								(void)luaScript->ReloadScript();
							isDirty_ = true;
							}
						}
						ImGui::EndCombo();
					}

					// エラー表示
					if (luaScript->HasError()) {
						auto& error = luaScript->GetLastError();
						if (error) {
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
							ImGui::TextWrapped(U8("エラー: %s"), error->message.c_str());
							ImGui::PopStyleColor();
						}
					}
				}
			}

			// コンポーネント追加ボタン
			ImGui::Spacing();
			ImGui::Separator();
			float buttonWidth = ImGui::GetContentRegionAvail().x;
			if (ImGui::Button(U8("コンポーネント追加"), ImVec2(buttonWidth, 0))) {
				ImGui::OpenPopup("AddComponentPopup");
			}

			if (ImGui::BeginPopup("AddComponentPopup")) {
				if (ImGui::MenuItem(U8("オーディオソース")) && !selected->GetComponent<AudioSource>()) {
					selected->AddComponent<AudioSource>();
					isDirty_ = true;
				}
				if (ImGui::MenuItem(U8("オーディオリスナー")) && !selected->GetComponent<AudioListener>()) {
					selected->AddComponent<AudioListener>();
					isDirty_ = true;
				}
				if (ImGui::MenuItem(U8("Luaスクリプト")) && !selected->GetComponent<LuaScriptComponent>()) {
					selected->AddComponent<LuaScriptComponent>();
					isDirty_ = true;
				}
				ImGui::EndPopup();
			}
		} else {
			ImGui::TextDisabled("No object selected");
		}

		ImGui::End();
	}

	void EditorUI::RenderConsoleAndDebugger() {
		ImGui::Begin(U8("コンソール"));

		// タブバー（コンソール / デバッガー）
		if (ImGui::BeginTabBar("ConsoleDebuggerTabs")) {
			// コンソール タブ
			if (ImGui::BeginTabItem(U8("コンソール"))) {
				// ツールバー
				if (ImGui::Button(U8("クリア"))) {
					consoleMessages_.clear();
				}
				ImGui::SameLine();
				static bool showInfo = true;
				static bool showWarning = true;
				static bool showError = true;
				ImGui::Checkbox(U8("情報"), &showInfo);
				ImGui::SameLine();
				ImGui::Checkbox(U8("警告"), &showWarning);
				ImGui::SameLine();
				ImGui::Checkbox(U8("エラー"), &showError);

				ImGui::Separator();

				// ログ表示
				ImGui::BeginChild("ConsoleLog", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
				for (const auto& msg : consoleMessages_) {
					// カラー分け
					ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
					if (msg.find("[Error]") != std::string::npos) {
						if (!showError) continue;
						color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
					} else if (msg.find("[Warning]") != std::string::npos) {
						if (!showWarning) continue;
						color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
					} else {
						if (!showInfo) continue;
					}

					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::TextUnformatted(msg.c_str());
					ImGui::PopStyleColor();
				}
				if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
					ImGui::SetScrollHereY(1.0f);
				}
				ImGui::EndChild();

				ImGui::EndTabItem();
			}

			// デバッガー タブ
			if (ImGui::BeginTabItem(U8("デバッガー"))) {
				// FPSグラフ
				static float fpsHistory[90] = {};
				static int fpsOffset = 0;
				static float updateTimer = 0.0f;

				updateTimer += ImGui::GetIO().DeltaTime;
				if (updateTimer >= 0.1f) {
					fpsHistory[fpsOffset] = ImGui::GetIO().Framerate;
					fpsOffset = (fpsOffset + 1) % 90;
					updateTimer = 0.0f;
				}

				ImGui::Text(U8("FPS: %.1f"), ImGui::GetIO().Framerate);
				ImGui::PlotLines("##FPS", fpsHistory, 90, fpsOffset, nullptr, 0.0f, 120.0f, ImVec2(0, 60));

				ImGui::Separator();
				ImGui::Text(U8("フレーム時間: %.3f ms"), 1000.0f / ImGui::GetIO().Framerate);

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	// ============================================================
	// 以下は既存の関数（互換性のため残す）
	// ============================================================

	void EditorUI::RenderHierarchy(const EditorContext& context) {
		if (!showHierarchy_) return;

		ImGui::Begin(U8("ヒエラルキー"), &showHierarchy_);

		// ヘッダーバー
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.0f));
		ImGui::Text(U8("シーンオブジェクト"));
		ImGui::PopStyleColor();
		ImGui::Separator();

		// 選択解除ボタン
		if (selectedObject_ && ImGui::SmallButton(U8("選択解除"))) {
			selectedObject_ = nullptr;
		}
		ImGui::SameLine();
		if (context.gameObjects) {
			ImGui::TextDisabled(U8("(%zu オブジェクト)"), context.gameObjects->size());
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
								isDirty_ = true;
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
								isDirty_ = true;
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

					// オブジェクトごとにキャッシュされたオイラー角を使用
					// （ジンバルロック問題を回避するため、ユーザー入力値を保持）
					uint64_t objId = reinterpret_cast<uint64_t>(obj);
					if (cachedEulerAngles_.find(objId) == cachedEulerAngles_.end()) {
						// 初回はクォータニオンから計算
						// クォータニオンを正規化してNaN防止
						rot = rot.Normalize();
						float qx = rot.GetX(), qy = rot.GetY(), qz = rot.GetZ(), qw = rot.GetW();

						// クォータニオンが有効かチェック
						float qLen = qx*qx + qy*qy + qz*qz + qw*qw;
						if (qLen < 0.0001f || std::isnan(qLen)) {
							// 無効なクォータニオン -> デフォルト値
							cachedEulerAngles_[objId] = Vector3(0.0f, 0.0f, 0.0f);
						} else {
							// XMQuaternionRotationRollPitchYaw互換のオイラー角抽出
							// 回転順序: Z(Roll) -> X(Pitch) -> Y(Yaw)
							float sinX = 2.0f * (qw * qx - qy * qz);
							float cosX = 1.0f - 2.0f * (qx * qx + qz * qz);
							float xRad = std::atan2(sinX, cosX);

							float sinY = 2.0f * (qw * qy + qx * qz);
							sinY = std::clamp(sinY, -1.0f, 1.0f);
							float yRad = std::asin(sinY);

							float sinZ = 2.0f * (qw * qz - qx * qy);
							float cosZ = 1.0f - 2.0f * (qy * qy + qz * qz);
							float zRad = std::atan2(sinZ, cosZ);

							constexpr float RAD_TO_DEG = 57.2957795f;
							float xDeg = xRad * RAD_TO_DEG;
							float yDeg = yRad * RAD_TO_DEG;
							float zDeg = zRad * RAD_TO_DEG;

							// NaNチェック
							if (std::isnan(xDeg) || std::isnan(yDeg) || std::isnan(zDeg)) {
								cachedEulerAngles_[objId] = Vector3(0.0f, 0.0f, 0.0f);
							} else {
								cachedEulerAngles_[objId] = Vector3(xDeg, yDeg, zDeg);
							}
						}
					}

					Vector3& cachedEuler = cachedEulerAngles_[objId];
					float euler[3] = { cachedEuler.GetX(), cachedEuler.GetY(), cachedEuler.GetZ() };

					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

					// Position（ドラッグ＆Ctrl+クリックで直接入力）
					float posArr[3] = { pos.GetX(), pos.GetY(), pos.GetZ() };
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Pos", posArr, 0.1f, 0.0f, 0.0f, "%.2f")) {
						transform.SetLocalPosition(Vector3(posArr[0], posArr[1], posArr[2]));
					}
					if (ImGui::IsItemActivated()) {
						BeginInspectorEdit(obj);
					}
					if (ImGui::IsItemDeactivatedAfterEdit()) {
						EndInspectorEdit();
					}

					// Rotation（ドラッグ＆Ctrl+クリックで直接入力）
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Rot", euler, 1.0f, 0.0f, 0.0f, "%.1f")) {
						// キャッシュを更新
						cachedEuler = Vector3(euler[0], euler[1], euler[2]);

						// Euler角（度）からQuaternionへ変換
						constexpr float DEG_TO_RAD = 0.0174532925f;
						float radX = euler[0] * DEG_TO_RAD;  // Pitch (X軸)
						float radY = euler[1] * DEG_TO_RAD;  // Yaw (Y軸)
						float radZ = euler[2] * DEG_TO_RAD;  // Roll (Z軸)
						transform.SetLocalRotation(Quaternion::RotationRollPitchYaw(radX, radY, radZ));
					}
					if (ImGui::IsItemActivated()) {
						BeginInspectorEdit(obj);
					}
					if (ImGui::IsItemDeactivatedAfterEdit()) {
						EndInspectorEdit();
					}

					// Scale（ドラッグ＆Ctrl+クリックで直接入力）
					float scaleArr[3] = { scale.GetX(), scale.GetY(), scale.GetZ() };
					ImGui::SetNextItemWidth(180.0f);
					if (ImGui::DragFloat3("Scale", scaleArr, 0.01f, 0.001f, 100.0f, "%.3f")) {
						transform.SetLocalScale(Vector3(scaleArr[0], scaleArr[1], scaleArr[2]));
					}
					if (ImGui::IsItemActivated()) {
						BeginInspectorEdit(obj);
					}
					if (ImGui::IsItemDeactivatedAfterEdit()) {
						EndInspectorEdit();
					}

					ImGui::PopStyleColor();

					if (isGizmoActive) {
						ImGui::EndDisabled();
					}

					// === モデル情報 ===
					auto* skinnedRenderer = obj->GetComponent<SkinnedMeshRenderer>();
					auto* meshRenderer = obj->GetComponent<MeshRenderer>();

					if (skinnedRenderer && skinnedRenderer->HasModel()) {
						ImGui::Separator();
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
						ImGui::Text(U8("スキンモデル"));
						ImGui::PopStyleColor();
						ImGui::Indent(10.0f);

						const auto& meshes = skinnedRenderer->GetMeshes();
						uint32 totalVertices = 0;
						uint32 totalIndices = 0;
						size_t totalMemory = 0;

						for (const auto& mesh : meshes) {
							uint32 vCount = mesh.GetVertexBuffer().GetVertexCount();
							uint32 iCount = mesh.GetIndexBuffer().GetIndexCount();
							totalVertices += vCount;
							totalIndices += iCount;
							// メモリ計算: SkinnedVertex(64bytes) + Index(4bytes)
							totalMemory += vCount * sizeof(SkinnedVertex) + iCount * sizeof(uint32);
						}

						ImGui::TextDisabled(U8("メッシュ数: %zu"), meshes.size());
						ImGui::TextDisabled(U8("頂点数: %u"), totalVertices);
						ImGui::TextDisabled(U8("三角形数: %u"), totalIndices / 3);

						// メモリサイズを適切な単位で表示
						if (totalMemory >= 1024 * 1024) {
							ImGui::TextDisabled(U8("メモリ: %.2f MB"), totalMemory / (1024.0f * 1024.0f));
						} else if (totalMemory >= 1024) {
							ImGui::TextDisabled(U8("メモリ: %.2f KB"), totalMemory / 1024.0f);
						} else {
							ImGui::TextDisabled(U8("メモリ: %zu B"), totalMemory);
						}

						// ボーン数
						auto* modelData = skinnedRenderer->GetModelData();
						if (modelData && modelData->skeleton) {
							ImGui::TextDisabled(U8("ボーン数: %zu"), modelData->skeleton->GetBoneCount());
						}

						// アニメーション数
						if (modelData && !modelData->animations.empty()) {
							ImGui::TextDisabled(U8("アニメーション数: %zu"), modelData->animations.size());
						}

						ImGui::Unindent(10.0f);
					}
					else if (meshRenderer && meshRenderer->GetMesh()) {
						ImGui::Separator();
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.8f, 1.0f));
						ImGui::Text(U8("静的モデル"));
						ImGui::PopStyleColor();
						ImGui::Indent(10.0f);

						const auto* mesh = meshRenderer->GetMesh();
						uint32 vCount = mesh->GetVertexBuffer().GetVertexCount();
						uint32 iCount = mesh->GetIndexBuffer().GetIndexCount();
						// メモリ計算: Vertex(32bytes) + Index(4bytes)
						size_t totalMemory = vCount * sizeof(Vertex) + iCount * sizeof(uint32);

						ImGui::TextDisabled(U8("頂点数: %u"), vCount);
						ImGui::TextDisabled(U8("三角形数: %u"), iCount / 3);

						// メモリサイズを適切な単位で表示
						if (totalMemory >= 1024 * 1024) {
							ImGui::TextDisabled(U8("メモリ: %.2f MB"), totalMemory / (1024.0f * 1024.0f));
						} else if (totalMemory >= 1024) {
							ImGui::TextDisabled(U8("メモリ: %.2f KB"), totalMemory / 1024.0f);
						} else {
							ImGui::TextDisabled(U8("メモリ: %zu B"), totalMemory);
						}

						ImGui::Unindent(10.0f);
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
						isDirty_ = true;
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
						isDirty_ = true;
						}

						// ループ
						bool loop = audioSource->IsLooping();
						if (ImGui::Checkbox("Loop", &loop)) {
							audioSource->SetLoop(loop);
						isDirty_ = true;
						}

						// PlayOnAwake
						bool playOnAwake = audioSource->GetPlayOnAwake();
						if (ImGui::Checkbox("Play On Awake", &playOnAwake)) {
							audioSource->SetPlayOnAwake(playOnAwake);
						isDirty_ = true;
						}

						// 3Dオーディオ設定
						bool is3D = audioSource->Is3D();
						if (ImGui::Checkbox("3D Audio", &is3D)) {
							audioSource->Set3D(is3D);
						isDirty_ = true;
						}

						if (is3D) {
							float minDist = audioSource->GetMinDistance();
							float maxDist = audioSource->GetMaxDistance();
							ImGui::SetNextItemWidth(100.0f);
							if (ImGui::DragFloat("Min Distance", &minDist, 0.1f, 0.1f, 100.0f)) {
								audioSource->SetMinDistance(minDist);
							isDirty_ = true;
							}
							ImGui::SetNextItemWidth(100.0f);
							if (ImGui::DragFloat("Max Distance", &maxDist, 1.0f, 1.0f, 1000.0f)) {
								audioSource->SetMaxDistance(maxDist);
							isDirty_ = true;
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
					isDirty_ = true;
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

		ImGui::Begin(U8("統計情報"), &showStats_);

		// Performance section
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.72f, 0.89f, 1.0f));
		ImGui::Text(U8("パフォーマンス"));
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

		ImGui::Text(U8("フレーム時間:"));
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

		// シーン統計
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.72f, 0.89f, 1.0f));
		ImGui::Text(U8("シーン"));
		ImGui::PopStyleColor();
		ImGui::Separator();

		if (context.gameObjects) {
			ImGui::Text(U8("オブジェクト数:"));
			ImGui::SameLine(120.0f);
			ImGui::Text("%zu", context.gameObjects->size());
		}

		ImGui::Spacing();
		ImGui::Separator();

		// カメラ情報
		if (context.camera) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.72f, 0.89f, 1.0f));
			ImGui::Text(U8("カメラ"));
			ImGui::PopStyleColor();
			ImGui::Separator();

			auto pos = context.camera->GetPosition();
			ImGui::Text(U8("位置:"));
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

		ImGui::Begin(U8("コンソール (旧)"), &showConsole_);

		if (ImGui::Button(U8("クリア"))) {
			consoleMessages_.clear();
		}
		ImGui::SameLine();
		if (ImGui::Button(U8("テストログ追加"))) {
			consoleMessages_.push_back(U8("[情報] テストログメッセージ"));
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

		ImGui::Begin(U8("プロジェクト"), &showProject_);

		ImGui::Text(U8("アセット"));
		ImGui::Separator();

		// モデルフォルダをスキャン
		if (ImGui::TreeNode(U8("モデル"))) {
			// リフレッシュボタン
			if (ImGui::SmallButton(U8("更新"))) {
				RefreshModelPaths();
				consoleMessages_.push_back(U8("[エディタ] モデルリストを更新しました"));
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
				ImGui::TextDisabled(U8("(モデルなし)"));
			}
			
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("テクスチャ"))) {
			if (context.loadedTextures.empty()) {
				ImGui::TextDisabled(U8("(なし)"));
			}
			else {
				for (const auto& texture : context.loadedTextures) {
					ImGui::Selectable(texture.c_str());
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(U8("シーン"))) {
			if (context.currentSceneName.empty()) {
				ImGui::TextDisabled(U8("(なし)"));
			}
			else {
				ImGui::Selectable(context.currentSceneName.c_str());
			}
			ImGui::TreePop();
		}

		// オーディオフォルダをスキャン
		if (ImGui::TreeNode(U8("オーディオ"))) {
			if (ImGui::SmallButton(U8("更新##Audio"))) {
				RefreshAudioPaths();
				consoleMessages_.push_back(U8("[エディタ] オーディオリストを更新しました"));
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
				ImGui::TextDisabled(U8("(オーディオファイルなし)"));
			}

			ImGui::TreePop();
		}

		// スクリプトフォルダをスキャン
		if (ImGui::TreeNode(U8("スクリプト"))) {
			if (ImGui::SmallButton(U8("更新##Scripts"))) {
				RefreshScriptPaths();
				consoleMessages_.push_back(U8("[エディタ] スクリプトリストを更新しました"));
			}
			ImGui::Separator();

			if (cachedScriptPaths_.empty()) {
				RefreshScriptPaths();
			}

			for (size_t i = 0; i < cachedScriptPaths_.size(); ++i) {
				const auto& scriptPath = cachedScriptPaths_[i];
				std::filesystem::path p(scriptPath);
				std::string filename = p.filename().string();

				ImGui::PushID(static_cast<int>(i + 20000)); // 他とIDが被らないようにオフセット

				ImGui::Text("📜");
				ImGui::SameLine();

				if (ImGui::Selectable(filename.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
					if (ImGui::IsMouseDoubleClicked(0)) {
						// ダブルクリック: VSCodeで開く
						OpenScriptInVSCode(scriptPath);
						consoleMessages_.push_back("[Editor] Opening in VSCode: " + filename);
					} else {
						// シングルクリック: LuaScriptComponentがある選択中オブジェクトにセット
						if (selectedObject_) {
							if (auto* luaScript = selectedObject_->GetComponent<LuaScriptComponent>()) {
								luaScript->SetScriptPath(scriptPath);
								consoleMessages_.push_back("[Editor] Script set: " + filename);
							}
						}
					}
				}

				// ツールチップでフルパス表示
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(U8("%s\n(ダブルクリックでVSCodeで開く)"), scriptPath.c_str());
				}

				// ドラッグ＆ドロップソース
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
					ImGui::SetDragDropPayload("SCRIPT_PATH", &i, sizeof(size_t));
					ImGui::Text("📜 %s", filename.c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::PopID();
			}

			if (cachedScriptPaths_.empty()) {
				ImGui::TextDisabled(U8("(スクリプトなし)"));
			}

			ImGui::TreePop();
		}

		ImGui::End();
	}

	void EditorUI::RenderProfiler() {
		if (!showProfiler_) return;

		ImGui::Begin(U8("プロファイラー"), &showProfiler_);

		ImGui::Text(U8("パフォーマンスプロファイラー"));
		ImGui::Separator();

		static float values[90] = {};
		static int values_offset = 0;
		values[values_offset] = ImGui::GetIO().Framerate;
		values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);

		ImGui::PlotLines("FPS", values, IM_ARRAYSIZE(values), values_offset, nullptr, 0.0f, 120.0f, ImVec2(0, 80));

		ImGui::Separator();
		ImGui::Text(U8("ドローコール: N/A"));
		ImGui::Text(U8("頂点数: N/A"));
		ImGui::Text(U8("三角形数: N/A"));

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
			consoleMessages_.push_back(U8("[エディタ] ギズモ: 移動"));
		}

		// E: スケールギズモ
		if (ImGui::IsKeyPressed(ImGuiKey_E, false) && !io.KeyCtrl) {
			gizmoSystem_.SetOperation(GizmoOperation::Scale);
			consoleMessages_.push_back(U8("[エディタ] ギズモ: スケール"));
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
			consoleMessages_.push_back(U8("[エディタ] レイアウトをリセットしました"));
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
		isDirty_ = true;
		consoleMessages_.push_back(U8("[エディタ] 変更を記録しました"));
	}

	// Undo実行
	void EditorUI::PerformUndo() {
		if (undoStack_.empty()) {
			consoleMessages_.push_back(U8("[エディタ] 元に戻す操作がありません"));
			return;
		}

		TransformSnapshot snapshot = undoStack_.top();
		undoStack_.pop();

		if (snapshot.targetObject) {
			auto& transform = snapshot.targetObject->GetTransform();
			transform.SetLocalPosition(snapshot.position);
			transform.SetLocalRotation(snapshot.rotation);
			transform.SetLocalScale(snapshot.scale);
			consoleMessages_.push_back(U8("[エディタ] 元に戻しました"));
		}
		else {
			consoleMessages_.push_back(U8("[エディタ] 元に戻す失敗: オブジェクトが存在しません"));
		}
	}

	// インスペクター編集開始時にスナップショットを保存
	void EditorUI::BeginInspectorEdit(GameObject* obj) {
		if (!obj || isInspectorEditing_) return;

		auto& transform = obj->GetTransform();
		preInspectorSnapshot_.targetObject = obj;
		preInspectorSnapshot_.position = transform.GetLocalPosition();
		preInspectorSnapshot_.rotation = transform.GetLocalRotation();
		preInspectorSnapshot_.scale = transform.GetLocalScale();
		isInspectorEditing_ = true;
	}

	// インスペクター編集終了時にUndoスタックにpush
	void EditorUI::EndInspectorEdit() {
		if (!isInspectorEditing_) return;

		if (preInspectorSnapshot_.targetObject) {
			PushUndoSnapshot(preInspectorSnapshot_);
		}
		isInspectorEditing_ = false;
	}

	// シーン保存
	void EditorUI::SaveScene(const std::string& filepath) {
		if (!gameObjects_) {
			consoleMessages_.push_back(U8("[エディタ] エラー: 保存するオブジェクトがありません"));
			return;
		}

		// 保存前にPostProcessManagerの現在値をCameraComponentに同期
		SyncPostProcessParamsToCamera();

		if (SceneSerializer::SaveScene(*gameObjects_, filepath)) {
			consoleMessages_.push_back(U8("[エディタ] シーンを保存しました: ") + filepath);
			// EditorCameraの設定も保存
			editorCamera_.SaveSettings();
			consoleMessages_.push_back(U8("[エディタ] カメラ設定を保存しました"));
			isDirty_ = false;
		}
		else {
			consoleMessages_.push_back(U8("[エディタ] シーン保存に失敗: ") + filepath);
		}
	}

	// シーンロード
	void EditorUI::LoadScene(const std::string& filepath) {
		if (!gameObjects_) {
			consoleMessages_.push_back(U8("[エディタ] エラー: オブジェクトコンテナがありません"));
			return;
		}

		if (SceneSerializer::LoadScene(filepath, *gameObjects_)) {
			consoleMessages_.push_back(U8("[エディタ] シーンを読み込みました: ") + filepath);
			// ロード後、最初のオブジェクトを選択
			if (!gameObjects_->empty()) {
				selectedObject_ = (*gameObjects_)[0].get();
			}
			// CameraComponentのPostProcessパラメータをPostProcessManagerに同期
			SyncPostProcessParamsFromCamera();
		}
		else {
			consoleMessages_.push_back(U8("[エディタ] シーン読み込みに失敗: ") + filepath);
		}
	}

	// PostProcessManagerのパラメータをCameraComponentに同期（保存前）
	void EditorUI::SyncPostProcessParamsToCamera() {
		if (!gameObjects_ || !postProcessManager_) return;

		for (const auto& obj : *gameObjects_) {
			if (!obj) continue;
			auto* camComp = obj->GetComponent<CameraComponent>();
			if (!camComp) continue;

			// Vignetteパラメータを同期
			if (auto* vignette = postProcessManager_->GetVignette()) {
				camComp->SetVignetteParams(vignette->GetParams());
			}
			// Fisheyeパラメータを同期
			if (auto* fisheye = postProcessManager_->GetFisheye()) {
				camComp->SetFisheyeParams(fisheye->GetParams());
			}
			// Grayscaleパラメータを同期
			if (auto* grayscale = postProcessManager_->GetGrayscale()) {
				camComp->SetGrayscaleParams(grayscale->GetParams());
			}
			break;
		}
	}

	// CameraComponentのPostProcessパラメータをPostProcessManagerに同期（ロード後）
	void EditorUI::SyncPostProcessParamsFromCamera() {
		if (!gameObjects_ || !postProcessManager_) return;

		for (const auto& obj : *gameObjects_) {
			if (!obj) continue;
			auto* camComp = obj->GetComponent<CameraComponent>();
			if (!camComp) continue;

			// Vignetteパラメータを同期
			if (auto* vignette = postProcessManager_->GetVignette()) {
				vignette->SetParams(camComp->GetVignetteParams());
			}
			// Fisheyeパラメータを同期
			if (auto* fisheye = postProcessManager_->GetFisheye()) {
				fisheye->SetParams(camComp->GetFisheyeParams());
			}
			// Grayscaleパラメータを同期
			if (auto* grayscale = postProcessManager_->GetGrayscale()) {
				grayscale->SetParams(camComp->GetGrayscaleParams());
			}
			// 最初のCameraComponentを見つけたら終了
			break;
		}
	}

	// モデルD&D処理（パスから）
	void EditorUI::HandleModelDragDrop(const std::string& modelPath) {
		if (!gameObjects_ || !resourceManager_) {
			consoleMessages_.push_back(U8("[エディタ] エラー: オブジェクトを作成できません"));
			return;
		}

		// 遅延ロードキューに追加
		pendingModelLoads_.push_back(modelPath);
		consoleMessages_.push_back(U8("[エディタ] モデルを読み込みキューに追加: ") + modelPath);
	}

	// モデルD&D処理（インデックスから）
	void EditorUI::HandleModelDragDropByIndex(size_t modelIndex) {
		if (modelIndex < cachedModelPaths_.size()) {
			HandleModelDragDrop(cachedModelPaths_[modelIndex]);
		}
		else {
			consoleMessages_.push_back(U8("[エディタ] エラー: 無効なモデルインデックス"));
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

	void EditorUI::OpenScriptInVSCode(const std::string& scriptPath) {
		// 絶対パスに変換
		std::filesystem::path absPath = std::filesystem::absolute(scriptPath);
		std::string absPathStr = absPath.string();

		// ShellExecuteExでファイルをデフォルトアプリケーションで開く（非同期）
		SHELLEXECUTEINFOA sei = { sizeof(sei) };
		sei.fMask = SEE_MASK_ASYNCOK;  // 非同期実行
		sei.lpVerb = "open";
		sei.lpFile = absPathStr.c_str();
		sei.nShow = SW_SHOWNORMAL;
		ShellExecuteExA(&sei);

		// 監視リストに追加（まだ追加されていなければ）
		bool alreadyWatching = false;
		for (const auto& watched : watchedScripts_) {
			if (watched.path == scriptPath) {
				alreadyWatching = true;
				break;
			}
		}

		if (!alreadyWatching && std::filesystem::exists(scriptPath)) {
			WatchedScript ws;
			ws.path = scriptPath;
			ws.lastWriteTime = std::filesystem::last_write_time(scriptPath);
			watchedScripts_.push_back(ws);
			consoleMessages_.push_back("[Editor] Watching script: " + scriptPath);
		}
	}

	void EditorUI::UpdateScriptFileWatcher() {
		for (auto& watched : watchedScripts_) {
			if (!std::filesystem::exists(watched.path)) continue;

			auto currentTime = std::filesystem::last_write_time(watched.path);
			if (currentTime != watched.lastWriteTime) {
				watched.lastWriteTime = currentTime;

				// 変更されたスクリプトをリロード
				consoleMessages_.push_back("[Editor] Script modified, reloading: " + watched.path);

				// このスクリプトを使用しているLuaScriptComponentをリロード
				if (gameObjects_) {
					for (auto& obj : *gameObjects_) {
						if (auto* luaScript = obj->GetComponent<LuaScriptComponent>()) {
							if (luaScript->GetScriptPath() == watched.path) {
								(void)luaScript->ReloadScript();
							isDirty_ = true;
								consoleMessages_.push_back("[Editor] Reloaded script on: " + obj->GetName());
							}
						}
					}
				}
			}
		}
	}

	void EditorUI::ReloadModifiedScripts() {
		// 手動リロード用（必要に応じて呼び出し）
		if (gameObjects_) {
			for (auto& obj : *gameObjects_) {
				if (auto* luaScript = obj->GetComponent<LuaScriptComponent>()) {
					(void)luaScript->ReloadScript();
							isDirty_ = true;
				}
			}
		}
		consoleMessages_.push_back("[Editor] All scripts reloaded");
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

		// モデルをロード（自動判別）
		SkinnedModelData* skinnedModelData = nullptr;
		StaticModelData* staticModelData = nullptr;
		bool isSkinned = resourceManager_->LoadModel(modelPath, &skinnedModelData, &staticModelData);

		// アップロード完了
		resourceManager_->EndUpload();

			if (!skinnedModelData && !staticModelData) {
				consoleMessages_.push_back("[Editor] ERROR: Failed to load model: " + modelPath);
				continue;
			}

			consoleMessages_.push_back("[Editor] Model loaded successfully");

			// GameObjectを生成
			auto newObject = std::make_unique<GameObject>(modelName);

			if (isSkinned && skinnedModelData) {
				// スキンモデルの場合
				// AnimatorComponentを先に追加（SkinnedMeshRenderer::Awake()でリンクできるように）
				auto* animator = newObject->AddComponent<AnimatorComponent>();
				if (skinnedModelData->skeleton) {
					animator->Initialize(skinnedModelData->skeleton, skinnedModelData->animations);
					if (!skinnedModelData->animations.empty()) {
						std::string animName = skinnedModelData->animations[0]->GetName();
						animator->Play(animName, true);
						consoleMessages_.push_back("[Editor] Playing animation: " + animName);
					}
				}

				// SkinnedMeshRendererを追加（AnimatorComponentが既に存在するのでAwake()でリンクされる）
				auto* renderer = newObject->AddComponent<SkinnedMeshRenderer>();
				renderer->SetModel(modelPath);  // まずパスを設定
				renderer->SetModel(skinnedModelData);   // 次に実際のモデルデータを設定
			} else if (staticModelData) {
				// 静的モデルの場合
				// MeshRendererを追加
				auto* renderer = newObject->AddComponent<MeshRenderer>();
				renderer->SetModelPath(modelPath);  // パスを設定（シリアライズ用）
				// 静的モデルの最初のメッシュを設定
				if (!staticModelData->meshes.empty()) {
					renderer->SetMesh(&staticModelData->meshes[0]);
					if (staticModelData->meshes[0].HasMaterial()) {
						renderer->SetMaterial(const_cast<Material*>(staticModelData->meshes[0].GetMaterial()));
					}
				}
				consoleMessages_.push_back("[Editor] Loaded as static model");
			}

			// 選択状態にする
			selectedObject_ = newObject.get();

			// GameObjectsリストに追加
			gameObjects_->push_back(std::move(newObject));
			isDirty_ = true;

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

	// スクリーン座標からレイを飛ばしてオブジェクトを選択
	GameObject* EditorUI::PickObjectAtScreenPos(float screenX, float screenY) {
		if (!gameObjects_ || !editorCamera_.GetCamera()) {
			return nullptr;
		}

		Camera* camera = editorCamera_.GetCamera();

		// スクリーン座標をビューポート内の正規化座標に変換 (-1 to 1)
		float ndcX = ((screenX - sceneViewPosX_) / sceneViewSizeX_) * 2.0f - 1.0f;
		float ndcY = 1.0f - ((screenY - sceneViewPosY_) / sceneViewSizeY_) * 2.0f;

		// 逆投影行列と逆ビュー行列を取得
		Matrix4x4 invProj = camera->GetProjectionMatrix().Inverse();
		Matrix4x4 invView = camera->GetViewMatrix().Inverse();

		// NDC座標をビュー空間に変換
		Vector4 rayClipNear(ndcX, ndcY, 0.0f, 1.0f);
		Vector4 rayClipFar(ndcX, ndcY, 1.0f, 1.0f);

		Vector4 rayViewNear = invProj.TransformVector4(rayClipNear);
		Vector4 rayViewFar = invProj.TransformVector4(rayClipFar);

		// パースペクティブ除算
		if (std::abs(rayViewNear.GetW()) > 1e-6f) {
			rayViewNear = Vector4(
				rayViewNear.GetX() / rayViewNear.GetW(),
				rayViewNear.GetY() / rayViewNear.GetW(),
				rayViewNear.GetZ() / rayViewNear.GetW(),
				1.0f
			);
		}
		if (std::abs(rayViewFar.GetW()) > 1e-6f) {
			rayViewFar = Vector4(
				rayViewFar.GetX() / rayViewFar.GetW(),
				rayViewFar.GetY() / rayViewFar.GetW(),
				rayViewFar.GetZ() / rayViewFar.GetW(),
				1.0f
			);
		}

		// ワールド空間に変換
		Vector4 rayWorldNear = invView.TransformVector4(rayViewNear);
		Vector4 rayWorldFar = invView.TransformVector4(rayViewFar);

		Vector3 rayOrigin(rayWorldNear.GetX(), rayWorldNear.GetY(), rayWorldNear.GetZ());
		Vector3 rayEnd(rayWorldFar.GetX(), rayWorldFar.GetY(), rayWorldFar.GetZ());
		Vector3 rayDir = (rayEnd - rayOrigin).Normalize();

		// 全オブジェクトとの交差判定
		GameObject* closestObject = nullptr;
		float closestDistance = std::numeric_limits<float>::max();

		for (const auto& obj : *gameObjects_) {
			if (!obj || !obj->IsActive()) continue;

			// SkinnedMeshRendererを持つオブジェクトのバウンディングボックスを取得
			auto* renderer = obj->GetComponent<SkinnedMeshRenderer>();
			if (renderer && renderer->HasModel()) {
				// ローカルバウンディングボックスを取得
				BoundingBox localBounds = renderer->GetBounds();

				// バウンディングボックスが無効な場合はデフォルトサイズを使用
				if (!localBounds.IsValid()) {
					localBounds = BoundingBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f));
				}

				// ワールド変換行列を取得
				const Transform& transform = obj->GetTransform();
				Matrix4x4 worldMatrix = transform.GetWorldMatrix();
				Matrix4x4 invWorldMatrix = worldMatrix.Inverse();

				// レイをローカル空間に変換
				Vector4 localOrigin4 = invWorldMatrix.TransformVector4(Vector4(rayOrigin.GetX(), rayOrigin.GetY(), rayOrigin.GetZ(), 1.0f));
				Vector4 localDir4 = invWorldMatrix.TransformVector4(Vector4(rayDir.GetX(), rayDir.GetY(), rayDir.GetZ(), 0.0f));

				Vector3 localRayOrigin(localOrigin4.GetX(), localOrigin4.GetY(), localOrigin4.GetZ());
				Vector3 localRayDir = Vector3(localDir4.GetX(), localDir4.GetY(), localDir4.GetZ()).Normalize();

				// ローカル空間でレイとバウンディングボックスの交差判定
				float tMin, tMax;
				if (localBounds.IntersectsRay(localRayOrigin, localRayDir, tMin, tMax)) {
					// ワールド空間での距離を計算
					Vector3 hitPointLocal = localRayOrigin + localRayDir * tMin;
					Vector4 hitPointWorld4 = worldMatrix.TransformVector4(Vector4(hitPointLocal.GetX(), hitPointLocal.GetY(), hitPointLocal.GetZ(), 1.0f));
					Vector3 hitPointWorld(hitPointWorld4.GetX(), hitPointWorld4.GetY(), hitPointWorld4.GetZ());
					float distance = (hitPointWorld - rayOrigin).Length();

					if (distance < closestDistance) {
						closestDistance = distance;
						closestObject = obj.get();
					}
				}
			}
			else {
				// メッシュがないオブジェクトはデフォルトバウンディングボックスで判定
				BoundingBox defaultBounds(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 2.0f, 1.0f));

				const Transform& transform = obj->GetTransform();
				Matrix4x4 worldMatrix = transform.GetWorldMatrix();
				Matrix4x4 invWorldMatrix = worldMatrix.Inverse();

				Vector4 localOrigin4 = invWorldMatrix.TransformVector4(Vector4(rayOrigin.GetX(), rayOrigin.GetY(), rayOrigin.GetZ(), 1.0f));
				Vector4 localDir4 = invWorldMatrix.TransformVector4(Vector4(rayDir.GetX(), rayDir.GetY(), rayDir.GetZ(), 0.0f));

				Vector3 localRayOrigin(localOrigin4.GetX(), localOrigin4.GetY(), localOrigin4.GetZ());
				Vector3 localRayDir = Vector3(localDir4.GetX(), localDir4.GetY(), localDir4.GetZ()).Normalize();

				float tMin, tMax;
				if (defaultBounds.IntersectsRay(localRayOrigin, localRayDir, tMin, tMax)) {
					Vector3 hitPointLocal = localRayOrigin + localRayDir * tMin;
					Vector4 hitPointWorld4 = worldMatrix.TransformVector4(Vector4(hitPointLocal.GetX(), hitPointLocal.GetY(), hitPointLocal.GetZ(), 1.0f));
					Vector3 hitPointWorld(hitPointWorld4.GetX(), hitPointWorld4.GetY(), hitPointWorld4.GetZ());
					float distance = (hitPointWorld - rayOrigin).Length();

					if (distance < closestDistance) {
						closestDistance = distance;
						closestObject = obj.get();
					}
				}
			}
		}

		return closestObject;
	}

	// SceneViewでのクリック選択処理
	void EditorUI::HandleSceneViewPicking() {
		// Editモードでのみ有効
		if (editorMode_ == EditorMode::Play) return;

		// ギズモ操作中は選択しない
		if (gizmoSystem_.IsUsing() || gizmoSystem_.IsOver()) return;

		ImGuiIO& io = ImGui::GetIO();

		// 左クリックでオブジェクト選択
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			ImVec2 mousePos = io.MousePos;

			// SceneViewの範囲内かチェック
			if (mousePos.x >= sceneViewPosX_ && mousePos.x <= sceneViewPosX_ + sceneViewSizeX_ &&
				mousePos.y >= sceneViewPosY_ && mousePos.y <= sceneViewPosY_ + sceneViewSizeY_) {

				GameObject* picked = PickObjectAtScreenPos(mousePos.x, mousePos.y);
				if (picked) {
					selectedObject_ = picked;
					// オイラー角キャッシュをクリア（新しいオブジェクト選択時）
					cachedEulerAngles_.clear();
				}
			}
		}
	}

} // namespace UnoEngine
