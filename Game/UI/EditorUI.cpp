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
#include "../../Engine/Input/InputManager.h"
#include "../../Engine/Core/CollisionComponent.h"
#include "../../Engine/Graphics/PointLightComponent.h"
#include "../../Engine/Graphics/SpotLightComponent.h"
#include "../../Engine/Physics/RigidbodyComponent.h"
#include "../../Engine/Physics/MeshColliderComponent.h"
#include "../../Engine/Physics/CapsuleColliderComponent.h"
#include "../../Engine/Core/PrefabManager.h"
#include "../../Engine/Editor/ParticleEditor.h"
#include "../../Engine/Navigation/NavMeshManager.h"
#include "../../Engine/Navigation/NavAgentComponent.h"
#include "../../Engine/AI/EnemyDetectionComponent.h"
#include "../../Engine/Video/VideoPlayerComponent.h"
#include "../../Engine/Rendering/Renderer.h"
#include "../../Engine/Vegetation/GrassSystem.h"
#include "../../Engine/Vegetation/GrassRenderer.h"
#include "../GameApplication.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "../../Engine/UI/imgui_toggle.h"
#include "../../Engine/UI/imgui_toggle_presets.h"
#include "ImGuizmo.h"
#include <algorithm>
#include <cmath>
#include <fstream>

// C++20 u8リテラルをconst char*に変換するヘルパー
#define U8(str) reinterpret_cast<const char*>(u8##str)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <filesystem>

namespace UnoEngine {
	namespace {
		const std::filesystem::path kNavLogDir = R"(C:\Users\Unoryuto\Documents\navlog)";
		const std::filesystem::path kNavLogFile = kNavLogDir / "nav_agent_log.csv";

		std::string EscapeCsvValue(const std::string& value) {
			std::string out;
			out.reserve(value.size() + 2);
			out.push_back('"');
			for (char c : value) {
				if (c == '"') {
					out.push_back('"');
				}
				out.push_back(c);
			}
			out.push_back('"');
			return out;
		}

		// ─────────────────────────────────────────────
		// Inspector UI helpers
		// ─────────────────────────────────────────────

		// Colored component section header. Returns true when open.
		static bool DrawComponentHeader(const char* label, ImVec4 col, bool defaultOpen = true) {
			ImVec4 hov = { std::min(col.x + 0.12f, 1.0f), std::min(col.y + 0.12f, 1.0f),
			               std::min(col.z + 0.12f, 1.0f), col.w };
			ImGui::PushStyleColor(ImGuiCol_Header,        col);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hov);
			ImGui::PushStyleColor(ImGuiCol_HeaderActive,  col);
			ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
			bool open = ImGui::CollapsingHeader(label, flags);
			ImGui::PopStyleColor(3);
			if (open) ImGui::Spacing();
			return open;
		}

		// Label aligned to a fixed column (115 px), muted gray color.
		static void PropLabel(const char* label) {
			ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.72f, 1.0f), "%s", label);
			ImGui::SameLine(115.0f);
		}

		// Unity-style XYZ drag control with colored axis buttons.
		// Clicking an axis button resets that component to resetVal.
		// outActivated / outDeactivatedAfterEdit are optional undo-tracking hints.
		static bool Vec3Control(const char* strId, float* v, float speed = 0.1f,
		                        float resetVal = 0.0f,
		                        bool* outActivated = nullptr,
		                        bool* outDeactivatedAfterEdit = nullptr) {
			const float lineH  = ImGui::GetFrameHeight();
			const float btnW   = lineH + 2.0f;
			const float avail  = ImGui::GetContentRegionAvail().x;
			const float gap    = ImGui::GetStyle().ItemSpacing.x;
			const float fieldW = std::max(30.0f, (avail - btnW * 3.0f - gap * 2.0f) / 3.0f);

			ImGui::PushID(strId);
			bool changed = false;
			bool anyActivated = false, anyDeactivated = false;

			const char* axisLabel[3]  = { "X",     "Y",     "Z"     };
			const char* dragId[3]     = { "##Xv",  "##Yv",  "##Zv"  };
			const ImVec4 btnColor[3]  = {
				{0.80f, 0.10f, 0.15f, 1.0f},
				{0.20f, 0.65f, 0.20f, 1.0f},
				{0.10f, 0.25f, 0.80f, 1.0f},
			};
			const ImVec4 btnHover[3]  = {
				{0.90f, 0.20f, 0.25f, 1.0f},
				{0.30f, 0.75f, 0.30f, 1.0f},
				{0.20f, 0.35f, 0.90f, 1.0f},
			};

			for (int i = 0; i < 3; ++i) {
				if (i > 0) ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button,        btnColor[i]);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHover[i]);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,  btnColor[i]);
				if (ImGui::Button(axisLabel[i], ImVec2(btnW, lineH))) {
					v[i] = resetVal;
					changed = true;
				}
				ImGui::PopStyleColor(3);
				ImGui::SameLine(0.0f, 0.0f);
				ImGui::SetNextItemWidth(fieldW);
				if (ImGui::DragFloat(dragId[i], &v[i], speed)) changed = true;
				anyActivated  |= ImGui::IsItemActivated();
				anyDeactivated |= ImGui::IsItemDeactivatedAfterEdit();
			}

			if (outActivated)          *outActivated          = anyActivated;
			if (outDeactivatedAfterEdit) *outDeactivatedAfterEdit = anyDeactivated;
			ImGui::PopID();
			return changed;
		}
	}

	// ============================================================
	// Editor Commands (Undo/Redo)
	// ============================================================

	struct TransformCommand final : IEditorCommand {
		GameObject* object = nullptr;
		Vector3 oldPos, newPos;
		Quaternion oldRot, newRot;
		Vector3 oldScale, newScale;
		void Execute() override {
			auto& t = object->GetTransform();
			t.SetLocalPosition(newPos);
			t.SetLocalRotation(newRot);
			t.SetLocalScale(newScale);
		}
		void Undo() override {
			auto& t = object->GetTransform();
			t.SetLocalPosition(oldPos);
			t.SetLocalRotation(oldRot);
			t.SetLocalScale(oldScale);
		}
	};

	struct DeleteObjectCommand final : IEditorCommand {
		std::vector<UniquePtr<GameObject>>* gameObjects = nullptr;
		std::unique_ptr<GameObject> savedObject;
		size_t savedIndex = 0;
		GameObject* objectPtr = nullptr;
		GameObject** selectedObjectRef = nullptr;
		std::unordered_set<GameObject*>* expandedObjects = nullptr;

		DeleteObjectCommand(std::vector<UniquePtr<GameObject>>* gos, GameObject* obj,
			GameObject** sel, std::unordered_set<GameObject*>* exp)
			: gameObjects(gos), objectPtr(obj), selectedObjectRef(sel), expandedObjects(exp) {}

		void Execute() override {
			for (size_t i = 0; i < gameObjects->size(); ++i) {
				if ((*gameObjects)[i].get() == objectPtr) {
					savedObject = std::move((*gameObjects)[i]);
					savedIndex = i;
					gameObjects->erase(gameObjects->begin() + i);
					expandedObjects->erase(objectPtr);
					if (*selectedObjectRef == objectPtr) *selectedObjectRef = nullptr;
					return;
				}
			}
		}
		void Undo() override {
			size_t pos = std::min(savedIndex, gameObjects->size());
			gameObjects->insert(gameObjects->begin() + pos, std::move(savedObject));
			*selectedObjectRef = objectPtr;
		}
	};

	struct CreateObjectCommand final : IEditorCommand {
		std::vector<UniquePtr<GameObject>>* gameObjects = nullptr;
		std::unique_ptr<GameObject> savedObject;
		GameObject* objectPtr = nullptr;
		GameObject** selectedObjectRef = nullptr;
		std::unordered_set<GameObject*>* expandedObjects = nullptr;

		CreateObjectCommand(std::vector<UniquePtr<GameObject>>* gos, GameObject* obj,
			GameObject** sel, std::unordered_set<GameObject*>* exp)
			: gameObjects(gos), objectPtr(obj), selectedObjectRef(sel), expandedObjects(exp) {}

		void Execute() override {
			gameObjects->push_back(std::move(savedObject));
			*selectedObjectRef = objectPtr;
		}
		void Undo() override {
			for (size_t i = 0; i < gameObjects->size(); ++i) {
				if ((*gameObjects)[i].get() == objectPtr) {
					savedObject = std::move((*gameObjects)[i]);
					gameObjects->erase(gameObjects->begin() + i);
					expandedObjects->erase(objectPtr);
					if (*selectedObjectRef == objectPtr) *selectedObjectRef = nullptr;
					return;
				}
			}
		}
	};

	// バッチコマンド（複数操作をアトミックにUndo/Redo）
	struct BatchCommand final : IEditorCommand {
		std::vector<std::unique_ptr<IEditorCommand>> commands;

		void Execute() override {
			for (auto& cmd : commands) cmd->Execute();
		}
		void Undo() override {
			for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
				(*it)->Undo();
			}
		}
	};

	void EditorUI::ClearSelection() {
		selectedObjects_.clear();
		selectedObject_ = nullptr;
	}

	void EditorUI::SelectObject(GameObject* obj, bool addToSelection) {
		if (!addToSelection) {
			selectedObjects_.clear();
		}
		if (obj) {
			if (selectedObjects_.count(obj)) {
				// トグル: 既に選択中なら解除
				selectedObjects_.erase(obj);
				if (selectedObject_ == obj) {
					selectedObject_ = selectedObjects_.empty() ? nullptr : *selectedObjects_.begin();
				}
			} else {
				selectedObjects_.insert(obj);
				selectedObject_ = obj;
			}
		}
	}

	bool EditorUI::IsSelected(GameObject* obj) const {
		return selectedObjects_.count(obj) > 0;
	}

	void EditorUI::Initialize(GraphicsDevice* graphics) {
		graphics_ = graphics;

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

		// NavMesh非同期ベイク完了チェック
		if (navMeshBakeFuture_.valid() && 
			navMeshBakeFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
			bool result = navMeshBakeFuture_.get();
			if (result) {
				auto& navMeshManager = Navigation::NavMeshManager::Get();
				auto stats = navMeshManager.GetStats();
				AddConsoleMessage(std::format(
					"[NavMesh] Bake complete: polys={}, verts={}, time={:.2f}s",
					stats.polyCount, stats.vertexCount, stats.buildTimeSeconds
				));
				showRecastNavMesh_ = true;
				navMeshManager.SetDebugDrawEnabled(true);
				inspectorTabIndex_ = 1;
			} else {
				AddConsoleMessage("[NavMesh] Bake failed");
			}
		}

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
		RenderObjectProperties(context); // 新: 右側パネル（Object Properties）
		RenderConsoleAndDebugger();      // 新: 下部パネル（Console & Debugger）
		RenderHierarchy(context);        // 互換性のため残す
		RenderInspector(context);        // 互換性のため残す
		RenderStats(context);            // Stats（右側に統合予定）
		RenderProject(context);
		RenderProfiler();

		// パーティクルエディター描画
		if (particleEditor_) {
			particleEditor_->Draw();
		}

		// シネマティックエディター描画
		cinematicEditor_.RenderWindow();


		// ビルドダイアログ描画
		RenderBuildDialog();

		// NavMeshベイク中モーダル
		if (navMeshBaking_.load()) {
			ImGui::OpenPopup(U8("NavMesh ベイク中"));
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);

		if (ImGui::BeginPopupModal(U8("NavMesh ベイク中"), nullptr, 
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
			
			float progress = navMeshBakeProgress_.load();
			std::string stage;
			{
				std::lock_guard<std::mutex> lock(navMeshBakeMutex_);
				stage = navMeshBakeStage_;
			}

			ImGui::Text(U8("NavMeshを生成しています..."));
			ImGui::Spacing();

			ImGui::ProgressBar(progress, ImVec2(-1, 24), std::format("{:.0f}%%", progress * 100.0f).c_str());

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", stage.c_str());

			if (!navMeshBaking_.load()) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// エディタカメラの更新
		float deltaTime = ImGui::GetIO().DeltaTime;

		// シネマティックエディタ更新（再生中のカメラ移動）
		cinematicEditor_.Update(deltaTime);

		editorCamera_.SetMovementEnabled(true);
		editorCamera_.SetPlaying(IsPlaying());
		// Raw Inputデルタを渡す
		if (scene_ && scene_->GetInputManager()) {
			auto& mouse = scene_->GetInputManager()->GetMouse();
			editorCamera_.SetRawMouseDelta(
				static_cast<float>(mouse.GetRawDeltaX()),
				static_cast<float>(mouse.GetRawDeltaY()));
		}
		editorCamera_.Update(deltaTime);

		// MainCameraのCameraComponentにも再生状態を設定
		if (scene_) {
			if (auto* camComp = scene_->GetActiveCameraComponent()) {
				camComp->SetPlaying(IsPlaying());
			}
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
			// イントロシネマティックをリセット＆再生（Game Viewのカメラを使用）
			if (scene_) {
				auto* app = static_cast<GameApplication*>(scene_->GetApplication());
				if (app) {
					auto& player = app->GetIntroCinematicPlayer();
					player.SetCamera(scene_->GetActiveCamera());
					player.Stop();
					player.Play();
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

			// イントロシネマティックを停止
			if (scene_) {
				auto* app = static_cast<GameApplication*>(scene_->GetApplication());
				if (app) {
					app->GetIntroCinematicPlayer().Stop();
				}
			}

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
				{
					bool cinematicOpen = cinematicEditor_.IsOpen();
					if (ImGui::MenuItem(U8("シネマティックエディタ"), "Ctrl+Shift+C", &cinematicOpen)) {
						cinematicEditor_.SetOpen(cinematicOpen);
						if (cinematicOpen) {
							cinematicEditor_.SetSceneViewCamera(&sceneViewCamera_);
							cinematicEditor_.SetPreviewCamera(&sceneViewCamera_);
						}
					}
				}


				ImGui::Separator();
				if (ImGui::MenuItem(U8("レイアウトをリセット"), "Ctrl+Shift+R")) {
					dockingLayoutInitialized_ = false;
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu(U8("ビルド"))) {
				if (ImGui::MenuItem(U8("ゲームをエクスポート..."), "Ctrl+Shift+B")) {
					showBuildDialog_ = true;
				}
				ImGui::Separator();
				ImGui::TextDisabled(U8("出力先を選択してGame.exeを生成します"));
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu(U8("ナビゲーション"))) {
				auto& recastNavMesh = Navigation::NavMeshManager::Get();

				if (ImGui::MenuItem(U8("ベイク"), "Ctrl+B", false, scene_ != nullptr)) {
					BakeNavMesh();
				}

				if (ImGui::MenuItem(U8("クリア"), nullptr, false, recastNavMesh.IsBuilt())) {
					recastNavMesh.Shutdown();
					recastNavMesh.Initialize();
					showRecastNavMesh_ = false;
					AddConsoleMessage(U8("[NavMesh] クリアしました"));
				}

				ImGui::Separator();

				if (ImGui::MenuItem(U8("表示"), nullptr, &showRecastNavMesh_, recastNavMesh.IsBuilt())) {
					recastNavMesh.SetDebugDrawEnabled(showRecastNavMesh_);
				}

				if (ImGui::MenuItem(U8("設定..."))) {
					showRecastNavMeshSettings_ = true;
					inspectorTabIndex_ = 1;
				}

				ImGui::Separator();

				if (ImGui::MenuItem(U8("保存..."), nullptr, false, recastNavMesh.IsBuilt())) {
					OPENFILENAMEA ofn = {};
					char filename[MAX_PATH] = "";
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = nullptr;
					ofn.lpstrFilter = "NavMesh (*.navmesh)\0*.navmesh\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = filename;
					ofn.nMaxFile = MAX_PATH;
					ofn.lpstrDefExt = "navmesh";
					ofn.Flags = OFN_OVERWRITEPROMPT;

					if (GetSaveFileNameA(&ofn)) {
						if (recastNavMesh.SaveNavMesh(filename)) {
							AddConsoleMessage(U8("[NavMesh] 保存: ") + std::string(filename));
						} else {
							AddConsoleMessage(U8("[NavMesh] 保存失敗"));
						}
					}
				}

				if (ImGui::MenuItem(U8("読み込み..."))) {
					OPENFILENAMEA ofn = {};
					char filename[MAX_PATH] = "";
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = nullptr;
					ofn.lpstrFilter = "NavMesh (*.navmesh)\0*.navmesh\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = filename;
					ofn.nMaxFile = MAX_PATH;
					ofn.Flags = OFN_FILEMUSTEXIST;

					if (GetOpenFileNameA(&ofn)) {
						if (recastNavMesh.LoadNavMesh(filename)) {
							AddConsoleMessage(U8("[NavMesh] 読み込み: ") + std::string(filename));
							showRecastNavMesh_ = true;
							recastNavMesh.SetDebugDrawEnabled(true);
						} else {
							AddConsoleMessage(U8("[NavMesh] 読み込み失敗"));
						}
					}
				}

				ImGui::Separator();

				// ステータス
				if (recastNavMesh.IsBuilt()) {
					auto stats = recastNavMesh.GetStats();
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), U8("● ビルド済み"));
					ImGui::TextDisabled(U8("  %d ポリゴン / %.2f秒"),
						stats.polyCount, stats.buildTimeSeconds);
				} else {
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), U8("○ 未ビルド"));
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

			// ドックスペースを分割（左ヒエラルキー + 中央Scene/Game + 右プロパティ + 下部）
			ImGuiID dock_main, dock_bottom;
			ImGuiID dock_left, dock_viewport, dock_right;
			ImGuiID dock_project, dock_console;

			// メイン領域(70%) | 下部(30%)
			dock_main = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Up, 0.70f, nullptr, &dock_bottom);

			// メイン領域の左にヒエラルキー(15%)を切り出す
			dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.15f, nullptr, &dock_main);

			// 残りをビューポート(75%) | 右プロパティ(25%)に分割
			dock_viewport = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.75f, nullptr, &dock_right);

			// 下部を左(40%) | 右(60%)に分割
			dock_project = ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Left, 0.40f, nullptr, &dock_console);

			// ゲームを先にドック → シーンを後でドックしてアクティブにする
			ImGui::DockBuilderDockWindow(U8("ゲーム"), dock_viewport);
			ImGui::DockBuilderDockWindow(U8("シーン"), dock_viewport);

			// 左: ヒエラルキー
			ImGui::DockBuilderDockWindow(U8("ヒエラルキー"), dock_left);

			// 右: オブジェクトプロパティ
			ImGui::DockBuilderDockWindow(U8("プロパティ"), dock_right);

			// 下部左: プロジェクト
			ImGui::DockBuilderDockWindow(U8("プロジェクト"), dock_project);

			// 下部右: コンソール & デバッガ
			ImGui::DockBuilderDockWindow(U8("コンソール"), dock_console);

			// 旧ウィンドウ名も配置（互換性）
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
			ImGui::Checkbox(U8("グリッド表示"), &showGrid_);
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

			// 草ペイントが有効な場合はペイント処理を優先
			if (grassPaintActive_) {
				HandleGrassPainting();
			} else {
				// SceneViewでのクリック選択処理
				HandleSceneViewPicking();
			}

			// Scene View へのモデルD&Dターゲット
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_INDEX")) {
					size_t modelIdx = *static_cast<const size_t*>(payload->Data);
					// ドロップ位置をY=0平面に投影してワールド座標を計算
					ImVec2 mouse = ImGui::GetIO().MousePos;
					Camera* cam = editorCamera_.GetCamera();
					if (cam && sceneViewSizeX_ > 0 && sceneViewSizeY_ > 0) {
						float ndcX = ((mouse.x - sceneViewPosX_) / sceneViewSizeX_) * 2.0f - 1.0f;
						float ndcY = 1.0f - ((mouse.y - sceneViewPosY_) / sceneViewSizeY_) * 2.0f;
						Matrix4x4 invProj = cam->GetProjectionMatrix().Inverse();
						Matrix4x4 invView = cam->GetViewMatrix().Inverse();
						auto divW = [](const Vector4& v) {
							float w = std::abs(v.GetW()) > 1e-6f ? v.GetW() : 1.0f;
							return Vector4(v.GetX()/w, v.GetY()/w, v.GetZ()/w, 1.0f);
						};
						Vector4 nearW = divW(invProj.TransformVector4(Vector4(ndcX, ndcY, 0.0f, 1.0f)));
						Vector4 farW  = divW(invProj.TransformVector4(Vector4(ndcX, ndcY, 1.0f, 1.0f)));
						Vector3 rayOrigin(invView.TransformVector4(nearW).GetX(),
						                  invView.TransformVector4(nearW).GetY(),
						                  invView.TransformVector4(nearW).GetZ());
						Vector3 rayEnd   (invView.TransformVector4(farW).GetX(),
						                  invView.TransformVector4(farW).GetY(),
						                  invView.TransformVector4(farW).GetZ());
						Vector3 rayDir = (rayEnd - rayOrigin).Normalize();
						// Y=0平面との交点。交差しない場合はカメラ前方10m
						Vector3 dropPos;
						if (std::abs(rayDir.GetY()) > 1e-4f) {
							float t = -rayOrigin.GetY() / rayDir.GetY();
							if (t > 0.0f) {
								dropPos = Vector3(
									rayOrigin.GetX() + rayDir.GetX() * t,
									0.0f,
									rayOrigin.GetZ() + rayDir.GetZ() * t
								);
							} else {
								dropPos = rayOrigin + rayDir * 10.0f;
							}
						} else {
							dropPos = rayOrigin + rayDir * 10.0f;
						}
						pendingDropPosition_ = dropPos;
					}
					HandleModelDragDropByIndex(modelIdx);
				}
				ImGui::EndDragDropTarget();
			}

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
					auto& t = selectedObject_->GetTransform();
				auto cmd = std::make_unique<TransformCommand>();
				cmd->object   = selectedObject_;
				cmd->oldPos   = preGizmoSnapshot_.position;
				cmd->oldRot   = preGizmoSnapshot_.rotation;
				cmd->oldScale = preGizmoSnapshot_.scale;
				cmd->newPos   = t.GetLocalPosition();
				cmd->newRot   = t.GetLocalRotation();
				cmd->newScale = t.GetLocalScale();
				PushExecutedCommand(std::move(cmd));
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

			// Playモード時のマウスロック＋FPS視点操作（シネマティック再生中は抑制）
			bool cinematicPlaying = false;
			if (scene_) {
				auto* app = static_cast<GameApplication*>(scene_->GetApplication());
				if (app) cinematicPlaying = app->IsIntroCinematicPlaying();
			}
			if (editorMode_ == EditorMode::Play && !cinematicPlaying) {
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

				// CameraComponentにマウスロック状態を渡す
				if (scene_) {
					if (auto* camComp = scene_->GetActiveCameraComponent()) {
						camComp->SetMouseLocked(gameViewMouseLocked_, gameViewLockMousePos_.x, gameViewLockMousePos_.y);
					}
				}

				// マウスロック中の視点操作とWASD移動
				// CameraComponentが一人称/三人称モードの場合はスキップ（CameraComponentに任せる）
				bool cameraComponentHandlesInput = false;
				if (scene_) {
					if (auto* camComp = scene_->GetActiveCameraComponent()) {
						if (camComp->GetViewMode() != CameraViewMode::Free) {
							cameraComponentHandlesInput = true;
						}
					}
				}

				if (gameViewMouseLocked_ && gameCamera_ && !cameraComponentHandlesInput) {
					// Raw Inputデルタを使用（高精度・フレームレート非依存）
					float deltaX = 0.0f;
					float deltaY = 0.0f;
					if (scene_ && scene_->GetInputManager()) {
						auto& mouse = scene_->GetInputManager()->GetMouse();
						deltaX = static_cast<float>(mouse.GetRawDeltaX());
						deltaY = static_cast<float>(mouse.GetRawDeltaY());
					}

					// カーソルをロック位置に戻す
					SetCursorPos(gameViewLockMousePos_.x, gameViewLockMousePos_.y);

					// 感度（マウスデルタはフレーム間移動量なのでdeltaTimeを掛けない）
					float sensitivity = editorCamera_.GetRotateSpeed() * 0.01f;

					gameViewYaw_ += deltaX * sensitivity;
					gameViewPitch_ += deltaY * sensitivity;

					// ピッチ制限
					const float maxPitch = 1.5f;
					if (gameViewPitch_ > maxPitch) gameViewPitch_ = maxPitch;
					if (gameViewPitch_ < -maxPitch) gameViewPitch_ = -maxPitch;

					// カメラの向きを更新
					Quaternion rotY = Quaternion::RotationAxis(Vector3::UnitY(), gameViewYaw_);
					Quaternion rotX = Quaternion::RotationAxis(Vector3::UnitX(), gameViewPitch_);
					gameCamera_->SetRotation(rotY * rotX);

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

		// タブバー
		if (ImGui::BeginTabBar("InspectorTabs")) {
			// オブジェクトタブ
			if (ImGui::BeginTabItem(U8("オブジェクト"))) {
				inspectorTabIndex_ = 0;
				RenderObjectInspectorTab(context);
				ImGui::EndTabItem();
			}

			// NavMeshタブ（ビルド済みまたは設定表示中のみ表示）
			auto& navMesh = Navigation::NavMeshManager::Get();
			if (navMesh.IsBuilt() || showRecastNavMeshSettings_) {
				ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
				if (inspectorTabIndex_ == 1) {
					flags |= ImGuiTabItemFlags_SetSelected;
				}
				if (ImGui::BeginTabItem("NavMesh", nullptr, flags)) {
					inspectorTabIndex_ = 1;
					RenderNavMeshInspectorTab();
					ImGui::EndTabItem();
				}
			}

			// 草ペイントタブ
			{
				ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
				if (inspectorTabIndex_ == 2) {
					flags |= ImGuiTabItemFlags_SetSelected;
				}
				if (ImGui::BeginTabItem(U8("草原"), nullptr, flags)) {
					inspectorTabIndex_ = 2;
					RenderGrassPaintTab();
					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	// ============================================================
	// 新しいUnity風パネル
	// ============================================================



	void EditorUI::RenderObjectProperties(const EditorContext& context) {
		ImGui::Begin(U8("プロパティ"));

		GameObject* selected = selectedObject_ ? selectedObject_ : context.player;

		if (selected) {
			// ヘッダー（オブジェクト名）
			bool isActive = selected->IsActive();
			if (ImGui::Checkbox("##active", &isActive)) {
				selected->SetActive(isActive);
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.75f, 1.0f));
			ImGui::TextUnformatted(selected->GetName().c_str());
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Transform セクション
			if (DrawComponentHeader(U8("  トランスフォーム"), {0.15f, 0.40f, 0.52f, 0.85f})) {
				auto& transform = selected->GetTransform();
				Vector3 pos   = transform.GetLocalPosition();
				Vector3 scale = transform.GetLocalScale();

				// Position
				float posArr[3] = { pos.GetX(), pos.GetY(), pos.GetZ() };
				PropLabel(U8("位置"));
				{
					bool activated = false, deactivated = false;
					if (Vec3Control("##Position", posArr, 0.1f, 0.0f, &activated, &deactivated)) {
						transform.SetLocalPosition(Vector3(posArr[0], posArr[1], posArr[2]));
					}
					if (activated)   BeginInspectorEdit(selected);
					if (deactivated) EndInspectorEdit();
				}
				ImGui::Spacing();

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

				PropLabel(U8("回転"));
				{
					bool activated = false, deactivated = false;
					if (Vec3Control("##Rotation", euler, 1.0f, 0.0f, &activated, &deactivated)) {
						cachedEuler = Vector3(euler[0], euler[1], euler[2]);
						constexpr float DEG_TO_RAD = 0.0174532925f;
						transform.SetLocalRotation(Quaternion::RotationRollPitchYaw(
							euler[0] * DEG_TO_RAD, euler[1] * DEG_TO_RAD, euler[2] * DEG_TO_RAD));
					}
					if (activated)   BeginInspectorEdit(selected);
					if (deactivated) EndInspectorEdit();
				}
				ImGui::Spacing();

				// Scale
				float scaleArr[3] = { scale.GetX(), scale.GetY(), scale.GetZ() };
				PropLabel(U8("スケール"));
				{
					bool activated = false, deactivated = false;
					if (Vec3Control("##Scale", scaleArr, 0.01f, 1.0f, &activated, &deactivated)) {
						transform.SetLocalScale(Vector3(scaleArr[0], scaleArr[1], scaleArr[2]));
					}
					if (activated)   BeginInspectorEdit(selected);
					if (deactivated) EndInspectorEdit();
				}
				ImGui::Spacing();
			}

			// エディターカメラ追従セクション
			if (DrawComponentHeader(U8("  エディターカメラ"), {0.30f, 0.30f, 0.30f, 0.85f}, false)) {
				bool isFollowing = (editorCamera_.GetFollowTarget() == selected);
				if (ImGui::Checkbox(U8("カメラ追従"), &isFollowing)) {
					if (isFollowing) {
						editorCamera_.SetFollowTarget(selected);
					} else {
						editorCamera_.SetFollowTarget(nullptr);
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(U8("Play中にエディターカメラがこのオブジェクトを上から追従します"));
				}

				if (isFollowing) {
					float height = editorCamera_.GetFollowHeight();
					ImGui::Text(U8("追従高さ"));
					ImGui::SameLine(80.0f);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##FollowHeight", &height, 0.5f, 1.0f, 100.0f)) {
						editorCamera_.SetFollowHeight(height);
					}
				}
			}

			// ── 当たり判定セクション ──
			{
				auto* collision = selected->GetComponent<CollisionComponent>();
				auto* meshCol   = selected->GetComponent<MeshColliderComponent>();
				auto* capsuleCol = selected->GetComponent<CapsuleColliderComponent>();
				bool hasAnyCollider = collision || meshCol || capsuleCol;

				if (hasAnyCollider) {
					if (DrawComponentHeader(U8("  当たり判定"), {0.55f, 0.28f, 0.08f, 0.85f})) {

						// ── 形状タイプ切り替え (AABB / メッシュ) ──
						bool hasMeshCol = meshCol != nullptr;
						int shapeType = hasMeshCol ? 1 : 0;  // 0=AABB, 1=メッシュ
						const char* shapeNames[] = { "AABB", U8("メッシュ") };
						ImGui::Text(U8("形状"));
						ImGui::SameLine(100.0f);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::Combo("##CollShape", &shapeType, shapeNames, 2)) {
							if (shapeType == 1 && !hasMeshCol) {
								selected->AddComponent<MeshColliderComponent>();
								if (!collision) {
									selected->AddComponent<CollisionComponent>();
									collision = selected->GetComponent<CollisionComponent>();
								}
							} else if (shapeType == 0 && hasMeshCol) {
								selected->RemoveComponent<MeshColliderComponent>();
								meshCol = nullptr;
							}
							isDirty_ = true;
						}

						// ── 共通設定 (CollisionComponent) ──
						if (collision) {
							bool enabled = collision->IsEnabled();
							ImGui::Text(U8("有効"));
							ImGui::SameLine(100.0f);
							if (ImGui::Checkbox("##CollEnabled", &enabled)) {
								collision->SetEnabled(enabled);
								if (meshCol) meshCol->SetEnabled(enabled);
								isDirty_ = true;
							}

							bool isColliding = collision->IsColliding();
							ImGui::Text(U8("衝突中"));
							ImGui::SameLine(100.0f);
							ImGui::TextColored(isColliding ? ImVec4(1, 0, 0, 1) : ImVec4(0, 1, 0, 1),
								isColliding ? U8("はい") : U8("いいえ"));

							bool isTrigger = collision->IsTrigger();
							ImGui::Text(U8("トリガー"));
							ImGui::SameLine(100.0f);
							if (ImGui::Checkbox("##IsTrigger", &isTrigger)) {
								collision->SetTrigger(isTrigger); isDirty_ = true;
							}

							bool isStatic = collision->IsStatic();
							ImGui::Text(U8("静的"));
							ImGui::SameLine(100.0f);
							if (ImGui::Checkbox("##IsStatic", &isStatic)) {
								collision->SetStatic(isStatic); isDirty_ = true;
							}

							NavMeshAreaType navArea = collision->GetNavMeshArea();
							const char* navAreaNames[] = { U8("なし"), U8("歩行可能") };
							int navAreaIdx = static_cast<int>(navArea);
							ImGui::Text(U8("NavMesh"));
							ImGui::SameLine(100.0f);
							ImGui::SetNextItemWidth(-1);
							if (ImGui::Combo("##NavMeshArea", &navAreaIdx, navAreaNames, 2)) {
								collision->SetNavMeshArea(static_cast<NavMeshAreaType>(navAreaIdx));
								isDirty_ = true;
							}
						}

						ImGui::Separator();

						// ── 形状別パラメータ ──
						meshCol = selected->GetComponent<MeshColliderComponent>();
						if (meshCol) {
							// メッシュコライダー詳細
							ImGui::TextDisabled(U8("三角形数: %u"), meshCol->GetTriangleCount());
							ImGui::TextDisabled(meshCol->IsBuilt() ? U8("BVH: 構築済み") : U8("BVH: 未構築"));
							if (ImGui::Button(U8("BVH再構築"))) {
								meshCol->RebuildBVH(); isDirty_ = true;
							}
						} else if (collision) {
							// AABB詳細
							bool autoSize = collision->IsAutoSized();
							ImGui::Text(U8("自動サイズ"));
							ImGui::SameLine(100.0f);
							if (ImGui::Checkbox("##AutoSize", &autoSize)) {
								collision->SetAutoSize(autoSize);
								if (autoSize) collision->RecalculateFromMesh();
								isDirty_ = true;
							}

							const auto& aabb = collision->GetLocalAABB();
							float aabbMin[3] = { aabb.min.GetX(), aabb.min.GetY(), aabb.min.GetZ() };
							float aabbMax[3] = { aabb.max.GetX(), aabb.max.GetY(), aabb.max.GetZ() };

							ImGui::Text(U8("最小"));
							ImGui::SameLine(100.0f);
							ImGui::SetNextItemWidth(-1);
							if (!autoSize) {
								if (ImGui::DragFloat3("##AABBMin", aabbMin, 0.01f)) {
									collision->SetLocalAABB(
										Vector3(aabbMin[0], aabbMin[1], aabbMin[2]),
										Vector3(aabbMax[0], aabbMax[1], aabbMax[2]));
									isDirty_ = true;
								}
							} else {
								ImGui::Text("%.2f, %.2f, %.2f", aabbMin[0], aabbMin[1], aabbMin[2]);
							}

							ImGui::Text(U8("最大"));
							ImGui::SameLine(100.0f);
							ImGui::SetNextItemWidth(-1);
							if (!autoSize) {
								if (ImGui::DragFloat3("##AABBMax", aabbMax, 0.01f)) {
									collision->SetLocalAABB(
										Vector3(aabbMin[0], aabbMin[1], aabbMin[2]),
										Vector3(aabbMax[0], aabbMax[1], aabbMax[2]));
									isDirty_ = true;
								}
							} else {
								ImGui::Text("%.2f, %.2f, %.2f", aabbMax[0], aabbMax[1], aabbMax[2]);
							}

							if (ImGui::Button(U8("メッシュから再計算"))) {
								collision->RecalculateFromMesh(); isDirty_ = true;
							}
						}

						// ── カプセルコライダー (キャラクター用) ──
						ImGui::Separator();
						if (capsuleCol) {
							ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), U8("カプセル"));

							bool ccEnabled = capsuleCol->IsEnabled();
							if (ImGui::Checkbox(U8("有効##CC"), &ccEnabled)) {
								capsuleCol->SetEnabled(ccEnabled); isDirty_ = true;
							}
							float base[3] = { capsuleCol->GetLocalBase().GetX(), capsuleCol->GetLocalBase().GetY(), capsuleCol->GetLocalBase().GetZ() };
							ImGui::Text(U8("下端")); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
							if (ImGui::DragFloat3("##CCBase", base, 0.01f)) {
								capsuleCol->SetLocalBase(Vector3(base[0], base[1], base[2])); isDirty_ = true;
							}
							float tip[3] = { capsuleCol->GetLocalTip().GetX(), capsuleCol->GetLocalTip().GetY(), capsuleCol->GetLocalTip().GetZ() };
							ImGui::Text(U8("上端")); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
							if (ImGui::DragFloat3("##CCTip", tip, 0.01f)) {
								capsuleCol->SetLocalTip(Vector3(tip[0], tip[1], tip[2])); isDirty_ = true;
							}
							float radius = capsuleCol->GetRadius();
							ImGui::Text(U8("半径")); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
							if (ImGui::DragFloat("##CCRadius", &radius, 0.01f, 0.01f, 10.0f)) {
								capsuleCol->SetRadius(radius); isDirty_ = true;
							}
							float stepHeight = capsuleCol->GetMaxStepHeight();
							ImGui::Text(U8("段差高さ")); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
							if (ImGui::DragFloat("##CCStepHeight", &stepHeight, 0.01f, 0.0f, 5.0f, "%.2f m")) {
								capsuleCol->SetMaxStepHeight(stepHeight); isDirty_ = true;
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip(U8("登れる階段の最大段差の高さ"));
							}
							if (ImGui::Button(U8("カプセル削除"))) {
								selected->RemoveComponent<CapsuleColliderComponent>(); isDirty_ = true;
							}
						} else {
							if (ImGui::Button(U8("カプセル追加"))) {
								selected->AddComponent<CapsuleColliderComponent>(); isDirty_ = true;
							}
							ImGui::SameLine();
							ImGui::TextDisabled(U8("(キャラクターに必要)"));
						}
					}
				} else {
					// 当たり判定コンポーネントが何もない場合
					if (DrawComponentHeader(U8("  当たり判定"), {0.38f, 0.38f, 0.38f, 0.85f}, false)) {
						ImGui::TextDisabled(U8("(当たり判定なし)"));
						if (ImGui::Button(U8("AABB追加"))) {
							selected->AddComponent<CollisionComponent>(); isDirty_ = true;
						}
						ImGui::SameLine();
						if (ImGui::Button(U8("メッシュ追加"))) {
							selected->AddComponent<CollisionComponent>();
							selected->AddComponent<MeshColliderComponent>();
							isDirty_ = true;
						}
						ImGui::SameLine();
						if (ImGui::Button(U8("カプセル追加"))) {
							selected->AddComponent<CapsuleColliderComponent>(); isDirty_ = true;
						}
					}
				}
			}

			// ── 物理セクション ──
			if (auto* rb = selected->GetComponent<RigidbodyComponent>()) {
				if (DrawComponentHeader(U8("  物理"), {0.20f, 0.50f, 0.80f, 0.85f})) {
					float rbMass = rb->GetMass();
					ImGui::Text(U8("質量")); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##RBMass", &rbMass, 0.01f, 0.001f, 1000.0f)) {
						rb->SetMass(rbMass); isDirty_ = true;
					}
					float rbDrag = rb->GetDrag();
					ImGui::Text(U8("抗力")); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##RBDrag", &rbDrag, 0.001f, 0.0f, 10.0f)) {
						rb->SetDrag(rbDrag); isDirty_ = true;
					}
					bool rbUseGrav = rb->UseGravity();
					if (ImGui::Checkbox(U8("重力"), &rbUseGrav)) {
						rb->SetUseGravity(rbUseGrav); isDirty_ = true;
					}
					bool rbKinematic = rb->IsKinematic();
					if (ImGui::Checkbox(U8("キネマティック"), &rbKinematic)) {
						rb->SetKinematic(rbKinematic); isDirty_ = true;
					}
					auto rbVel = rb->GetVelocity();
					ImGui::TextDisabled(U8("速度: (%.2f, %.2f, %.2f)"), rbVel.GetX(), rbVel.GetY(), rbVel.GetZ());
					ImGui::TextDisabled(rb->IsGrounded() ? U8("接地: はい") : U8("接地: いいえ"));
				}
			} else {
				if (DrawComponentHeader(U8("  物理"), {0.38f, 0.38f, 0.38f, 0.85f}, false)) {
					ImGui::TextDisabled(U8("(物理なし)"));
					if (ImGui::Button(U8("物理追加"))) {
						selected->AddComponent<RigidbodyComponent>(); isDirty_ = true;
					}
				}
			}

			// DirectionalLight section
			if (auto* dl = selected->GetComponent<DirectionalLightComponent>()) {
				if (DrawComponentHeader(U8("  Directional Light"), {0.95f, 0.85f, 0.20f, 0.85f})) {
					auto dlCol = dl->GetColor();
					float dlC[3] = { dlCol.GetX(), dlCol.GetY(), dlCol.GetZ() };
					ImGui::Text("Color"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::ColorEdit3("##DLColor", dlC)) {
						dl->SetColor(Vector3(dlC[0], dlC[1], dlC[2])); isDirty_ = true;
					}
					float dlInt = dl->GetIntensity();
					ImGui::Text("Intensity"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##DLInt", &dlInt, 0.01f, 0.0f, 100.0f)) {
						dl->SetIntensity(dlInt); isDirty_ = true;
					}
					auto dir = dl->GetDirection();
					ImGui::Text("Direction"); ImGui::SameLine(100.0f);
					ImGui::TextDisabled("(%.2f, %.2f, %.2f)", dir.GetX(), dir.GetY(), dir.GetZ());
					ImGui::TextDisabled(U8("  ※ Transformの回転で変更"));
				}
			}

			// PointLight section
			if (auto* pl = selected->GetComponent<PointLightComponent>()) {
				if (DrawComponentHeader(U8("  PointLight"), {0.85f, 0.75f, 0.10f, 0.85f})) {
					auto plCol = pl->GetColor();
					float plC[3] = { plCol.GetX(), plCol.GetY(), plCol.GetZ() };
					ImGui::Text("Color"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::ColorEdit3("##PLColor", plC)) {
						pl->SetColor(Vector3(plC[0], plC[1], plC[2])); isDirty_ = true;
					}
					float plInt = pl->GetIntensity();
					ImGui::Text("Intensity"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##PLInt", &plInt, 0.01f, 0.0f, 100.0f)) {
						pl->SetIntensity(plInt); isDirty_ = true;
					}
					float plRange = pl->GetRange();
					ImGui::Text("Range"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##PLRange", &plRange, 0.1f, 0.0f, 1000.0f)) {
						pl->SetRange(plRange); isDirty_ = true;
					}
				}
			}

			// SpotLight section
			if (auto* sl = selected->GetComponent<SpotLightComponent>()) {
				if (DrawComponentHeader(U8("  SpotLight"), {0.85f, 0.50f, 0.10f, 0.85f})) {
					auto slCol = sl->GetColor();
					float slC[3] = { slCol.GetX(), slCol.GetY(), slCol.GetZ() };
					ImGui::Text("Color"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::ColorEdit3("##SLColor", slC)) {
						sl->SetColor(Vector3(slC[0], slC[1], slC[2])); isDirty_ = true;
					}
					float slInt = sl->GetIntensity();
					ImGui::Text("Intensity"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##SLInt", &slInt, 0.01f, 0.0f, 100.0f)) {
						sl->SetIntensity(slInt); isDirty_ = true;
					}
					float slRange = sl->GetRange();
					ImGui::Text("Range"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##SLRange", &slRange, 0.1f, 0.0f, 1000.0f)) {
						sl->SetRange(slRange); isDirty_ = true;
					}
					float slSpot = sl->GetSpotAngle();
					ImGui::Text("Outer Angle"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::SliderFloat("##SLOuter", &slSpot, 0.0f, 90.0f)) {
						sl->SetSpotAngle(slSpot); isDirty_ = true;
					}
					float slInner = sl->GetInnerAngle();
					ImGui::Text("Inner Angle"); ImGui::SameLine(100.0f); ImGui::SetNextItemWidth(-1);
					if (ImGui::SliderFloat("##SLInner", &slInner, 0.0f, slSpot)) {
						sl->SetInnerAngle(slInner); isDirty_ = true;
					}
				}
			}

			// Prefab
			ImGui::Separator();
			if (ImGui::Button("Save as Prefab")) {
				std::string prefabPath = "assets/prefabs/" + selected->GetName() + ".prefab";
				PrefabManager::SavePrefab(selected, prefabPath);
				AddConsoleMessage("[Prefab] Saved: " + prefabPath);
			}

			// Camera セクション
			if (auto* camComp = selected->GetComponent<CameraComponent>()) {
				if (DrawComponentHeader(U8("  カメラ"), {0.10f, 0.22f, 0.62f, 0.85f})) {
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

					// カメラ追従設定
					ImGui::Text(U8("カメラ追従"));
					ImGui::Indent(20.0f);
					
					// 視点モード
					CameraViewMode viewMode = camComp->GetViewMode();
					const char* viewModeNames[] = { U8("自由"), U8("一人称"), U8("三人称") };
					int viewModeIdx = static_cast<int>(viewMode);
					ImGui::Text(U8("視点モード"));
					ImGui::SameLine(100.0f);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::Combo("##ViewMode", &viewModeIdx, viewModeNames, 3)) {
						camComp->SetViewMode(static_cast<CameraViewMode>(viewModeIdx));
						isDirty_ = true;
					}

					if (viewMode != CameraViewMode::Free) {
						// ターゲットオブジェクト選択
						std::string targetName = camComp->GetFollowTargetName();
						std::string displayTarget = targetName.empty() ? U8("(なし)") : targetName;
						ImGui::Text(U8("ターゲット"));
						ImGui::SameLine(100.0f);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::BeginCombo("##FollowTarget", displayTarget.c_str())) {
							if (ImGui::Selectable(U8("(なし)"), targetName.empty())) {
								camComp->SetFollowTargetName("");
								isDirty_ = true;
							}
							if (gameObjects_) {
								for (auto& obj : *gameObjects_) {
									if (obj.get() == selected) continue;
									bool isSelected = (obj->GetName() == targetName);
									if (ImGui::Selectable(obj->GetName().c_str(), isSelected)) {
										camComp->SetFollowTargetName(obj->GetName());
										isDirty_ = true;
									}
								}
							}
							ImGui::EndCombo();
						}

						// スムーズ
						float smoothness = camComp->GetFollowSmoothness();
						ImGui::Text(U8("滑らかさ"));
						ImGui::SameLine(100.0f);
						ImGui::SetNextItemWidth(-1);
						if (ImGui::SliderFloat("##FollowSmooth", &smoothness, 1.0f, 30.0f)) {
							camComp->SetFollowSmoothness(smoothness);
							isDirty_ = true;
						}

						if (viewMode == CameraViewMode::FirstPerson) {
									// 一人称視点オフセット
									Vector3 offset = camComp->GetFirstPersonOffset();
									float offsetArr[3] = { offset.GetX(), offset.GetY(), offset.GetZ() };
									ImGui::Text(U8("目の位置"));
									ImGui::SameLine(100.0f);
									ImGui::SetNextItemWidth(-1);
									if (ImGui::DragFloat3("##FPOffset", offsetArr, 0.1f)) {
										camComp->SetFirstPersonOffset(Vector3(offsetArr[0], offsetArr[1], offsetArr[2]));
										isDirty_ = true;
									}

									// マウス感度
									float sensitivity = camComp->GetMouseSensitivity();
									ImGui::Text(U8("マウス感度"));
									ImGui::SameLine(100.0f);
									ImGui::SetNextItemWidth(-1);
									if (ImGui::SliderFloat("##MouseSens", &sensitivity, 0.05f, 1.0f)) {
										camComp->SetMouseSensitivity(sensitivity);
										isDirty_ = true;
									}

									// 移動速度
									float moveSpeed = camComp->GetFirstPersonMoveSpeed();
									ImGui::Text(U8("移動速度"));
									ImGui::SameLine(100.0f);
									ImGui::SetNextItemWidth(-1);
									if (ImGui::SliderFloat("##FPMoveSpeed", &moveSpeed, 1.0f, 20.0f)) {
										camComp->SetFirstPersonMoveSpeed(moveSpeed);
										isDirty_ = true;
									}

								// ターゲットモデルを非表示にするか
								bool hideTarget = camComp->GetHideTargetInFirstPerson();
								if (ImGui::Checkbox(U8("ターゲットモデル非表示"), &hideTarget)) {
									camComp->SetHideTargetInFirstPerson(hideTarget);
									isDirty_ = true;
								}
							} else {
							// 三人称視点設定
							float distance = camComp->GetFollowDistance();
							ImGui::Text(U8("距離"));
							ImGui::SameLine(100.0f);
							ImGui::SetNextItemWidth(-1);
							if (ImGui::SliderFloat("##FollowDist", &distance, 1.0f, 30.0f)) {
								camComp->SetFollowDistance(distance);
								isDirty_ = true;
							}

							float height = camComp->GetFollowHeight();
							ImGui::Text(U8("高さ"));
							ImGui::SameLine(100.0f);
							ImGui::SetNextItemWidth(-1);
							if (ImGui::SliderFloat("##FollowHeight", &height, 0.0f, 20.0f)) {
								camComp->SetFollowHeight(height);
								isDirty_ = true;
							}

							float pitch = camComp->GetFollowPitch();
							ImGui::Text(U8("見下ろし角"));
							ImGui::SameLine(100.0f);
							ImGui::SetNextItemWidth(-1);
							if (ImGui::SliderFloat("##FollowPitch", &pitch, 0.0f, 60.0f)) {
								camComp->SetFollowPitch(pitch);
								isDirty_ = true;
							}
						}
					}
					ImGui::Unindent(20.0f);
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
								else if (effectType == PostProcessType::PS1) {
									if (auto* ps1 = postProcessManager_->GetPS1()) {
										auto& params = ps1->GetParams();
										ImGui::SetNextItemWidth(-1);
										int colorDepth = static_cast<int>(params.colorDepth);
										if (ImGui::SliderInt("##PS1ColorDepth", &colorDepth, 1, 8, U8("色深度: %d bit"))) {
											params.colorDepth = static_cast<uint32>(colorDepth);
											camComp->SetPS1Params(params);
											isDirty_ = true;
										}
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##PS1ResScale", &params.resolutionScale, 1.0f, 8.0f, U8("解像度: 1/%.1f"))) {
											camComp->SetPS1Params(params);
											isDirty_ = true;
										}
										ImGui::SetNextItemWidth(-1);
										if (ImGui::Checkbox(U8("ディザリング##PS1Dither"), &params.ditherEnabled)) {
											camComp->SetPS1Params(params);
											isDirty_ = true;
										}
										if (params.ditherEnabled) {
											ImGui::SetNextItemWidth(-1);
											if (ImGui::SliderFloat("##PS1DitherStr", &params.ditherStrength, 0.0f, 2.0f, U8("強度: %.2f"))) {
												camComp->SetPS1Params(params);
												isDirty_ = true;
											}
										}
									}
								}
								else if (effectType == PostProcessType::ChromaticAberration) {
									if (auto* chromatic = postProcessManager_->GetChromaticAberration()) {
										auto& params = chromatic->GetParams();
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##CAIntensity", &params.intensity, 0.0f, 1.0f, U8("強度: %.2f"))) {
											camComp->SetChromaticAberrationParams(params);
											isDirty_ = true;
										}
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##CARedOffset", &params.redOffset, -0.05f, 0.05f, U8("赤オフセット: %.3f"))) {
											camComp->SetChromaticAberrationParams(params);
											isDirty_ = true;
										}
										ImGui::SetNextItemWidth(-1);
										if (ImGui::SliderFloat("##CABlueOffset", &params.blueOffset, -0.05f, 0.05f, U8("青オフセット: %.3f"))) {
											camComp->SetChromaticAberrationParams(params);
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
				if (DrawComponentHeader(U8("  オーディオリスナー"), {0.38f, 0.12f, 0.52f, 0.85f})) {
					ImGui::TextDisabled(U8("(3Dオーディオのメインリスナー)"));
				}
			}

			// Audio Source セクション
			if (auto* audioSource = selected->GetComponent<AudioSource>()) {
				if (DrawComponentHeader(U8("  オーディオソース"), {0.38f, 0.12f, 0.52f, 0.85f})) {
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
				if (DrawComponentHeader(U8("  スクリプト"), {0.12f, 0.48f, 0.18f, 0.85f})) {
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

			// ドロップターゲット処理のラムダ
			auto handleDropTarget = [&]() {
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) {
						size_t index = *(const size_t*)payload->Data;
						if (index < cachedScriptPaths_.size()) {
							const std::string& scriptPath = cachedScriptPaths_[index];
							auto* luaScript = selected->GetComponent<LuaScriptComponent>();
							if (!luaScript) {
								luaScript = selected->AddComponent<LuaScriptComponent>();
								consoleMessages_.push_back("[Editor] Added LuaScriptComponent to: " + selected->GetName());
							}
							luaScript->SetScriptPath(scriptPath);
							(void)luaScript->ReloadScript();
							consoleMessages_.push_back("[Script] Set script: " + std::filesystem::path(scriptPath).filename().string());
							isDirty_ = true;
						}
					}
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_PATH")) {
						size_t index = *(const size_t*)payload->Data;
						if (index < cachedAudioPaths_.size()) {
							const std::string& audioPath = cachedAudioPaths_[index];
							auto* audioSource = selected->GetComponent<AudioSource>();
							if (!audioSource) {
								audioSource = selected->AddComponent<AudioSource>();
								consoleMessages_.push_back("[Editor] Added AudioSource to: " + selected->GetName());
							}
							audioSource->SetClipPath(audioPath);
							audioSource->LoadClip(audioPath);
							consoleMessages_.push_back("[Audio] Set clip: " + std::filesystem::path(audioPath).filename().string());
							isDirty_ = true;
						}
					}
					ImGui::EndDragDropTarget();
				}
			};

			// ドロップゾーン（上）
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::InvisibleButton("##PropertyDropZoneTop", ImVec2(ImGui::GetContentRegionAvail().x, 20.0f));
			handleDropTarget();
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(U8("スクリプトやオーディオをここにドロップ"));
			}

			// コンポーネント追加ボタン
			float buttonWidth = ImGui::GetContentRegionAvail().x;
			if (ImGui::Button(U8("コンポーネント追加"), ImVec2(buttonWidth, 0))) {
				ImGui::OpenPopup("AddComponentPopup");
			}

			// ドロップゾーン（下） - 残りの空白全体
			float remainingHeight = ImGui::GetContentRegionAvail().y;
			if (remainingHeight > 10.0f) {
				ImGui::InvisibleButton("##PropertyDropZoneBottom", ImVec2(ImGui::GetContentRegionAvail().x, remainingHeight));
				handleDropTarget();
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(U8("スクリプトやオーディオをここにドロップ"));
				}
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
				if (ImGui::MenuItem(U8("ビデオプレイヤー")) && !selected->GetComponent<VideoPlayerComponent>()) {
					auto* videoPlayer = selected->AddComponent<VideoPlayerComponent>();
					videoPlayer->SetGraphicsDevice(graphics_);
					isDirty_ = true;
				}
				if (ImGui::BeginMenu(U8("ライト"))) {
					if (ImGui::MenuItem("Directional Light") && !selected->GetComponent<DirectionalLightComponent>()) {
						auto* dl = selected->AddComponent<DirectionalLightComponent>();
						dl->UseTransformDirection(true);
						isDirty_ = true;
					}
					if (ImGui::MenuItem("Point Light") && !selected->GetComponent<PointLightComponent>()) {
						selected->AddComponent<PointLightComponent>();
						isDirty_ = true;
					}
					if (ImGui::MenuItem("Spot Light") && !selected->GetComponent<SpotLightComponent>()) {
						selected->AddComponent<SpotLightComponent>();
						isDirty_ = true;
					}
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}
		} else if (selectedGenerator_ != GeneratorType::None) {
			RenderGeneratorProperties();
		} else {
			ImGui::TextDisabled("No object selected");
		}

		ImGui::End();
	}

	void EditorUI::RenderGeneratorProperties() {
		if (selectedGenerator_ == GeneratorType::Grass) {
			// ===== 草原ジェネレーター（テクスチャベース・シンプル版） =====
			if (!renderer_) return;
			auto* grassSystem = renderer_->GetGrassSystem();
			auto* grassRenderer = renderer_->GetGrassRenderer();
			if (!grassSystem || !grassRenderer) return;

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
			ImGui::Text("🌿");
			ImGui::SameLine();
			ImGui::TextUnformatted(U8("草原ジェネレーター"));
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// テクスチャ
			if (ImGui::CollapsingHeader(U8("テクスチャ"), ImGuiTreeNodeFlags_DefaultOpen)) {
				const auto& currentPath = grassRenderer->GetTexturePath();
				if (currentPath.empty()) {
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), U8("(未設定 - 白テクスチャ)"));
				} else {
					namespace fs = std::filesystem;
					std::string filename = fs::path(currentPath).filename().string();
					ImGui::TextWrapped("%s", filename.c_str());
				}
				if (ImGui::Button(U8("テクスチャを選択..."), ImVec2(-1, 0))) {
					grassTextureBrowseOpen_ = true;
					grassTextureScanned_ = false;
				}
			}

			ImGui::Spacing();

			// サイズ
			if (ImGui::CollapsingHeader(U8("サイズ"), ImGuiTreeNodeFlags_DefaultOpen)) {
				float bw = grassRenderer->GetBaseWidth();
				float bh = grassRenderer->GetBaseHeight();
				if (ImGui::SliderFloat(U8("幅"), &bw, 0.1f, 5.0f, "%.2f")) {
					grassRenderer->SetBaseWidth(bw);
				}
				if (ImGui::SliderFloat(U8("高さ"), &bh, 0.1f, 5.0f, "%.2f")) {
					grassRenderer->SetBaseHeight(bh);
				}
				ImGui::SliderFloat(U8("最小スケール"), &grassMinScale_, 0.1f, 2.0f, "%.2f");
				ImGui::SliderFloat(U8("最大スケール"), &grassMaxScale_, 0.1f, 3.0f, "%.2f");
				ImGui::SliderFloat(U8("色変化"), &grassColorVariation_, 0.0f, 1.0f, "%.2f");
			}

			ImGui::Spacing();

			// ブラシ
			if (ImGui::CollapsingHeader(U8("ブラシ"), ImGuiTreeNodeFlags_DefaultOpen)) {
				bool isBrush = (grassPaintMode_ == GrassPaintMode::Brush);
				bool isStamp = (grassPaintMode_ == GrassPaintMode::Stamp);
				bool isErase = (grassPaintMode_ == GrassPaintMode::Erase);
				if (ImGui::RadioButton(U8("ブラシ"), isBrush)) grassPaintMode_ = GrassPaintMode::Brush;
				ImGui::SameLine();
				if (ImGui::RadioButton(U8("スタンプ"), isStamp)) grassPaintMode_ = GrassPaintMode::Stamp;
				ImGui::SameLine();
				if (ImGui::RadioButton(U8("消去"), isErase)) grassPaintMode_ = GrassPaintMode::Erase;

				ImGui::SliderFloat(U8("半径"), &grassBrushRadius_, 0.5f, 20.0f, "%.1f m");
				ImGui::SliderFloat(U8("密度"), &grassDensity_, 1.0f, 50.0f, "%.0f /m2");

				ImGui::Spacing();

				if (grassPaintActive_) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
					if (ImGui::Button(U8("ペイント ON"), ImVec2(-1, 28))) {
						grassPaintActive_ = false;
					}
					ImGui::PopStyleColor();
				} else {
					if (ImGui::Button(U8("ペイント OFF"), ImVec2(-1, 28))) {
						grassPaintActive_ = true;
					}
				}
			}

			ImGui::Spacing();

			// 風（シンプル版）
			if (ImGui::CollapsingHeader(U8("風"), ImGuiTreeNodeFlags_DefaultOpen)) {
				float wd = grassRenderer->GetWindDis();
				if (ImGui::SliderFloat(U8("風の強さ"), &wd, 0.0f, 1.0f, "%.2f")) {
					grassRenderer->SetWindDis(wd);
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// 統計
			ImGui::Text(U8("草の本数: %d"), grassSystem->GetInstanceCount());
			if (ImGui::Button(U8("全てクリア"), ImVec2(-1, 0))) {
				grassSystem->Clear();
			}

		} else if (selectedGenerator_ == GeneratorType::GodotGrass) {
			// ===== GodotGrass ジェネレーター =====
			RenderGodotGrassProperties();
		}
	}

	void EditorUI::RenderGodotGrassProperties() {
		if (!renderer_) return;
		auto* grassSystem = renderer_->GetGrassSystem();
		auto* grassRenderer = renderer_->GetGrassRenderer();
		if (!grassSystem || !grassRenderer) return;

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.6f, 1.0f));
		ImGui::Text("🌾");
		ImGui::SameLine();
		ImGui::TextUnformatted("GodotGrass");
		ImGui::PopStyleColor();
		ImGui::TextDisabled(U8("Godot風シェーダー草原"));

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// === カラー ===
		if (ImGui::CollapsingHeader(U8("カラー##godot"), ImGuiTreeNodeFlags_DefaultOpen)) {
			auto bc = grassRenderer->GetBottomColor();
			auto tc = grassRenderer->GetTopColor();
			auto cv1 = grassRenderer->GetColorVar1();
			auto cv2 = grassRenderer->GetColorVar2();
			float bottomCol[3] = { bc.x, bc.y, bc.z };
			float topCol[3]    = { tc.x, tc.y, tc.z };
			float var1Col[3]   = { cv1.x, cv1.y, cv1.z };
			float var2Col[3]   = { cv2.x, cv2.y, cv2.z };

			if (ImGui::ColorEdit3(U8("根元の色##g"), bottomCol)) {
				grassRenderer->SetBottomColor(bottomCol[0], bottomCol[1], bottomCol[2]);
			}
			if (ImGui::ColorEdit3(U8("先端の色##g"), topCol)) {
				grassRenderer->SetTopColor(topCol[0], topCol[1], topCol[2]);
			}
			if (ImGui::ColorEdit3(U8("バリエーション1##g"), var1Col)) {
				grassRenderer->SetColorVar1(var1Col[0], var1Col[1], var1Col[2]);
			}
			if (ImGui::ColorEdit3(U8("バリエーション2##g"), var2Col)) {
				grassRenderer->SetColorVar2(var2Col[0], var2Col[1], var2Col[2]);
			}
		}

		ImGui::Spacing();

		// === サイズ & 高さノイズ ===
		if (ImGui::CollapsingHeader(U8("サイズ##godot"), ImGuiTreeNodeFlags_DefaultOpen)) {
			float bw = grassRenderer->GetBaseWidth();
			float bh = grassRenderer->GetBaseHeight();
			if (ImGui::SliderFloat(U8("幅##g"), &bw, 0.1f, 5.0f, "%.2f")) {
				grassRenderer->SetBaseWidth(bw);
			}
			if (ImGui::SliderFloat(U8("高さ##g"), &bh, 0.1f, 5.0f, "%.2f")) {
				grassRenderer->SetBaseHeight(bh);
			}

			ImGui::Spacing();
			ImGui::TextDisabled(U8("ノイズベース高さ変動"));

			float minS = grassRenderer->GetCombinedNoiseMinScale();
			float maxS = grassRenderer->GetCombinedNoiseMaxScale();
			bool inv = grassRenderer->GetInvertCombinedNoise();
			if (ImGui::SliderFloat(U8("最小高さ##g"), &minS, 0.0f, 2.0f, "%.2f")) {
				grassRenderer->SetCombinedNoiseMinScale(minS);
			}
			if (ImGui::SliderFloat(U8("最大高さ##g"), &maxS, 0.0f, 3.0f, "%.2f")) {
				grassRenderer->SetCombinedNoiseMaxScale(maxS);
			}
			if (ImGui::Checkbox(U8("ノイズ反転##g"), &inv)) {
				grassRenderer->SetInvertCombinedNoise(inv);
			}
		}

		ImGui::Spacing();

		// === ブラシ ===
		if (ImGui::CollapsingHeader(U8("ブラシ##godot"), ImGuiTreeNodeFlags_DefaultOpen)) {
			bool isBrush = (grassPaintMode_ == GrassPaintMode::Brush);
			bool isStamp = (grassPaintMode_ == GrassPaintMode::Stamp);
			bool isErase = (grassPaintMode_ == GrassPaintMode::Erase);
			if (ImGui::RadioButton(U8("ブラシ##g"), isBrush)) grassPaintMode_ = GrassPaintMode::Brush;
			ImGui::SameLine();
			if (ImGui::RadioButton(U8("スタンプ##g"), isStamp)) grassPaintMode_ = GrassPaintMode::Stamp;
			ImGui::SameLine();
			if (ImGui::RadioButton(U8("消去##g"), isErase)) grassPaintMode_ = GrassPaintMode::Erase;

			ImGui::SliderFloat(U8("半径##g"), &grassBrushRadius_, 0.5f, 20.0f, "%.1f m");
			ImGui::SliderFloat(U8("密度##g"), &grassDensity_, 1.0f, 50.0f, "%.0f /m2");
			ImGui::SliderFloat(U8("最小スケール##g"), &grassMinScale_, 0.1f, 2.0f, "%.2f");
			ImGui::SliderFloat(U8("最大スケール##g"), &grassMaxScale_, 0.1f, 3.0f, "%.2f");

			ImGui::Spacing();

			if (grassPaintActive_) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
				if (ImGui::Button(U8("ペイント ON##g"), ImVec2(-1, 28))) {
					grassPaintActive_ = false;
				}
				ImGui::PopStyleColor();
			} else {
				if (ImGui::Button(U8("ペイント OFF##g"), ImVec2(-1, 28))) {
					grassPaintActive_ = true;
				}
			}
		}

		ImGui::Spacing();

		// === 風 ===
		if (ImGui::CollapsingHeader(U8("風##godot"), ImGuiTreeNodeFlags_DefaultOpen)) {
			float ws = grassRenderer->GetWindSpeed();
			float wd = grassRenderer->GetWindDis();
			float ns = grassRenderer->GetNoiseStrength();
			float ds = grassRenderer->GetDisplaceStrength();

			if (ImGui::SliderFloat(U8("風速##g"), &ws, 0.0f, 0.05f, "%.4f")) {
				grassRenderer->SetWindSpeed(ws);
			}
			if (ImGui::SliderFloat(U8("揺れ幅##g"), &wd, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetWindDis(wd);
			}
			if (ImGui::SliderFloat(U8("ノイズ強度##g"), &ns, 0.0f, 2.0f, "%.2f")) {
				grassRenderer->SetNoiseStrength(ns);
			}
			if (ImGui::SliderFloat(U8("変位強度##g"), &ds, 0.0f, 10.0f, "%.1f")) {
				grassRenderer->SetDisplaceStrength(ds);
			}

			ImGui::Spacing();
			ImGui::TextDisabled(U8("風ノイズテクスチャ"));

			float wns = grassRenderer->GetWindNoiseScale();
			float wpx = grassRenderer->GetWindNoisePanSpeedX();
			float wpy = grassRenderer->GetWindNoisePanSpeedY();
			float nf = grassRenderer->GetNoiseFloor();
			float wnss = grassRenderer->GetWindNoiseScaleStrength();

			if (ImGui::SliderFloat(U8("ノイズスケール##g"), &wns, 1.0f, 100.0f, "%.1f")) {
				grassRenderer->SetWindNoiseScale(wns);
			}
			float panSpeed[2] = { wpx, wpy };
			if (ImGui::SliderFloat2(U8("パン速度##g"), panSpeed, -0.1f, 0.1f, "%.3f")) {
				grassRenderer->SetWindNoisePanSpeed(panSpeed[0], panSpeed[1]);
			}
			if (ImGui::SliderFloat(U8("ノイズ床##g"), &nf, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetNoiseFloor(nf);
			}
			if (ImGui::SliderFloat(U8("高さ変動##g"), &wnss, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetWindNoiseScaleStrength(wnss);
			}
		}

		ImGui::Spacing();

		// === 風影 ===
		if (ImGui::CollapsingHeader(U8("風影##godot"), 0)) {
			auto wsc = grassRenderer->GetWindShadowColor();
			float shadowCol[3] = { wsc.x, wsc.y, wsc.z };
			if (ImGui::ColorEdit3(U8("影の色##g"), shadowCol)) {
				grassRenderer->SetWindShadowColor(shadowCol[0], shadowCol[1], shadowCol[2]);
			}
			float wss = grassRenderer->GetWindShadowStrength();
			if (ImGui::SliderFloat(U8("影の強さ##g"), &wss, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetWindShadowStrength(wss);
			}
			float wdt = grassRenderer->GetWindShadowDispThreshold();
			if (ImGui::SliderFloat(U8("閾値##g"), &wdt, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetWindShadowDispThreshold(wdt);
			}
			float wsm = grassRenderer->GetWindShadowSmoothing();
			if (ImGui::SliderFloat(U8("スムージング##g"), &wsm, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetWindShadowSmoothing(wsm);
			}
			float fss = grassRenderer->GetFlattenShadowStrength();
			if (ImGui::SliderFloat(U8("踏み倒し影##g"), &fss, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetFlattenShadowStrength(fss);
			}
		}

		ImGui::Spacing();

		// === 踏み倒し ===
		if (ImGui::CollapsingHeader(U8("踏み倒し##godot"), 0)) {
			float fr = grassRenderer->GetFlattenRadius();
			float fs = grassRenderer->GetFlattenStrength();
			float ff = grassRenderer->GetFlattenFloor();
			if (ImGui::SliderFloat(U8("範囲##g"), &fr, 0.1f, 5.0f, "%.2f")) {
				grassRenderer->SetFlattenRadius(fr);
			}
			if (ImGui::SliderFloat(U8("強さ##g"), &fs, 0.0f, 10.0f, "%.1f")) {
				grassRenderer->SetFlattenStrength(fs);
			}
			if (ImGui::SliderFloat(U8("最低高さ##g"), &ff, 0.0f, 1.0f, "%.2f")) {
				grassRenderer->SetFlattenFloor(ff);
			}
			ImGui::TextDisabled(U8("カメラ位置で踏み倒しテスト"));
		}

		ImGui::Spacing();

		// === ノイズスケール ===
		if (ImGui::CollapsingHeader(U8("ノイズ##godot"), 0)) {
			float n1s = grassRenderer->GetNoise1Scale();
			float n2s = grassRenderer->GetNoise2Scale();
			if (ImGui::SliderFloat(U8("ノイズ1スケール##g"), &n1s, 1.0f, 100.0f, "%.1f")) {
				grassRenderer->SetNoise1Scale(n1s);
			}
			if (ImGui::SliderFloat(U8("ノイズ2スケール##g"), &n2s, 1.0f, 100.0f, "%.1f")) {
				grassRenderer->SetNoise2Scale(n2s);
			}
		}

		ImGui::Spacing();

		// === テクスチャ（アルファカットアウト用） ===
		if (ImGui::CollapsingHeader(U8("テクスチャ##godot"), 0)) {
			const auto& currentPath = grassRenderer->GetTexturePath();
			if (currentPath.empty()) {
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), U8("(未設定 - 白テクスチャ)"));
			} else {
				namespace fs = std::filesystem;
				std::string filename = fs::path(currentPath).filename().string();
				ImGui::TextWrapped("%s", filename.c_str());
			}
			ImGui::TextDisabled(U8("アルファカットアウト形状用"));
			if (ImGui::Button(U8("テクスチャを選択...##g"), ImVec2(-1, 0))) {
				grassTextureBrowseOpen_ = true;
				grassTextureScanned_ = false;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// === 統計 ===
		ImGui::Text(U8("草の本数: %d"), grassSystem->GetInstanceCount());
		if (ImGui::Button(U8("全てクリア##g"), ImVec2(-1, 0))) {
			grassSystem->Clear();
		}
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
		if ((selectedObject_ || !selectedObjects_.empty()) && ImGui::SmallButton(U8("選択解除"))) {
			ClearSelection();
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
				else if (obj->GetComponent<DirectionalLightComponent>()) icon = "☀";
				else if (obj->GetComponent<PointLightComponent>()) icon = "💡";
				else if (obj->GetComponent<SpotLightComponent>()) icon = "🔦";
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
					if (IsSelected(obj) || selectedObject_ == obj) {
						flags |= ImGuiTreeNodeFlags_Selected;
					}

					ImGui::TreeNodeEx(obj->GetName().c_str(), flags);

					// シングルクリックで選択＋フォーカス
					if (ImGui::IsItemClicked() && !ImGui::IsMouseDoubleClicked(0)) {
						bool fHeld = ImGui::IsKeyDown(ImGuiKey_F);
						SelectObject(obj, fHeld);
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
							auto* audioSource = obj->GetComponent<AudioSource>();
							if (!audioSource) {
								audioSource = obj->AddComponent<AudioSource>();
								consoleMessages_.push_back("[Editor] Added AudioSource to: " + obj->GetName());
							}
							audioSource->SetClipPath(audioPath);
							audioSource->LoadClip(audioPath);
							consoleMessages_.push_back("[Audio] Set clip: " + std::filesystem::path(audioPath).filename().string());
							selectedObject_ = obj;
							isDirty_ = true;
						}
					}
					// ScriptファイルのD&Dターゲット（オブジェクトにドロップ）
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) {
						size_t index = *(const size_t*)payload->Data;
						if (index < cachedScriptPaths_.size()) {
							const std::string& scriptPath = cachedScriptPaths_[index];
							auto* luaScript = obj->GetComponent<LuaScriptComponent>();
							if (!luaScript) {
								luaScript = obj->AddComponent<LuaScriptComponent>();
								consoleMessages_.push_back("[Editor] Added LuaScriptComponent to: " + obj->GetName());
							}
							luaScript->SetScriptPath(scriptPath);
							(void)luaScript->ReloadScript();
							consoleMessages_.push_back("[Script] Set script: " + std::filesystem::path(scriptPath).filename().string());
							selectedObject_ = obj;
							isDirty_ = true;
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
							if (renamingObject_ == obj) renamingObject_ = nullptr;
						consoleMessages_.push_back("[Editor] Deleted object: " + obj->GetName());
						ExecuteCommand(std::make_unique<DeleteObjectCommand>(gameObjects_, obj, &selectedObject_, &expandedObjects_));
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
					if (ImGui::MenuItem("Collision (AABB)")) {
						if (!obj->GetComponent<CollisionComponent>()) {
							obj->AddComponent<CollisionComponent>();
							consoleMessages_.push_back("[Editor] Added CollisionComponent to: " + obj->GetName());
							isDirty_ = true;
						}
					}
					if (ImGui::BeginMenu("Light")) {
						if (ImGui::MenuItem("Directional Light") && !obj->GetComponent<DirectionalLightComponent>()) {
							auto* dl = obj->AddComponent<DirectionalLightComponent>();
							dl->UseTransformDirection(true);
							consoleMessages_.push_back("[Editor] Added DirectionalLight to: " + obj->GetName());
							isDirty_ = true;
						}
						if (ImGui::MenuItem("Point Light") && !obj->GetComponent<PointLightComponent>()) {
							obj->AddComponent<PointLightComponent>();
							consoleMessages_.push_back("[Editor] Added PointLight to: " + obj->GetName());
							isDirty_ = true;
						}
						if (ImGui::MenuItem("Spot Light") && !obj->GetComponent<SpotLightComponent>()) {
							obj->AddComponent<SpotLightComponent>();
							consoleMessages_.push_back("[Editor] Added SpotLight to: " + obj->GetName());
							isDirty_ = true;
						}
						ImGui::EndMenu();
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

					// === VideoPlayerComponent ===
					if (auto* videoPlayer = obj->GetComponent<VideoPlayerComponent>()) {
						ImGui::Separator();
						ImGui::Text("VideoPlayer");
						ImGui::Indent(10.0f);

						// GraphicsDeviceを設定（未設定の場合）
						if (graphics_) {
							videoPlayer->SetGraphicsDevice(graphics_);
						}

						// ビデオファイル選択
						std::string currentPath = videoPlayer->GetVideoPath();
						std::string displayName = currentPath.empty() ? "(None)" :
							std::filesystem::path(currentPath).filename().string();

						// ドラッグ&ドロップ可能なボタンで表示
						ImGui::Button(displayName.c_str(), ImVec2(180.0f, 0.0f));

						// ドラッグ&ドロップターゲット（プロジェクトパネルからのビデオファイル）
						if (ImGui::BeginDragDropTarget()) {
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VIDEO_PATH")) {
								std::string droppedPath(static_cast<const char*>(payload->Data));
								videoPlayer->LoadVideo(droppedPath);
								isDirty_ = true;
								consoleMessages_.push_back("[Editor] Video set: " + droppedPath);
							}
							ImGui::EndDragDropTarget();
						}

						ImGui::SameLine();
						if (ImGui::Button("...##VideoFile")) {
							char filename[MAX_PATH] = "";
							OPENFILENAMEA ofn = {};
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = nullptr;
							ofn.lpstrFilter = "Video Files\0*.mp4;*.avi;*.mkv;*.webm\0All Files\0*.*\0";
							ofn.lpstrFile = filename;
							ofn.nMaxFile = MAX_PATH;
							ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
							if (GetOpenFileNameA(&ofn)) {
								videoPlayer->LoadVideo(filename);
								isDirty_ = true;
							}
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Browse for video file");
						}

						// ターゲットマテリアル名
						std::string matName = videoPlayer->GetTargetMaterialName();
						char matNameBuf[256] = {};
						strncpy_s(matNameBuf, matName.c_str(), sizeof(matNameBuf) - 1);
						ImGui::SetNextItemWidth(180.0f);
						if (ImGui::InputText("Material", matNameBuf, sizeof(matNameBuf))) {
							videoPlayer->SetTargetMaterialByName(matNameBuf);
							isDirty_ = true;
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Material name to display video on");
						}

						// ビデオ情報表示
						if (!currentPath.empty()) {
							ImGui::TextDisabled("Size: %dx%d", videoPlayer->GetWidth(), videoPlayer->GetHeight());
							ImGui::TextDisabled("FPS: %.2f", videoPlayer->GetFrameRate());
							ImGui::TextDisabled("Duration: %.1fs", videoPlayer->GetDuration());
						}

						// ループ設定
						bool looping = videoPlayer->IsLooping();
						if (ImGui::Checkbox("Loop", &looping)) {
							videoPlayer->SetLooping(looping);
							isDirty_ = true;
						}

						// 再生コントロール
						if (videoPlayer->IsPlaying() && !videoPlayer->IsPaused()) {
							ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Playing");
							if (ImGui::Button("Pause##Video")) {
								videoPlayer->Pause();
							}
							ImGui::SameLine();
							if (ImGui::Button("Stop##Video")) {
								videoPlayer->Stop();
							}
						} else if (videoPlayer->IsPaused()) {
							ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "Paused");
							if (ImGui::Button("Resume##Video")) {
								videoPlayer->Play();
							}
							ImGui::SameLine();
							if (ImGui::Button("Stop##Video")) {
								videoPlayer->Stop();
							}
						} else {
							if (ImGui::Button("Play##Video")) {
								videoPlayer->Play();
							}
						}

						// シークバー
						if (!currentPath.empty()) {
							float currentTime = static_cast<float>(videoPlayer->GetCurrentTime());
							float duration = static_cast<float>(videoPlayer->GetDuration());
							ImGui::SetNextItemWidth(180.0f);
							if (ImGui::SliderFloat("##Seek", &currentTime, 0.0f, duration, "%.1fs")) {
								videoPlayer->Seek(static_cast<double>(currentTime));
							}
						}

						ImGui::Unindent(10.0f);
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
				consoleMessages_.push_back("[Editor] Deleted object (DEL): " + selectedObject_->GetName());
				ExecuteCommand(std::make_unique<DeleteObjectCommand>(gameObjects_, selectedObject_, &selectedObject_, &expandedObjects_));
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

		// Hierarchy背景の右クリックメニュー（オブジェクト作成）
		if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
			if (ImGui::MenuItem("Create Empty")) {
				auto newObj = std::make_unique<GameObject>();
				newObj->SetName("GameObject");
				auto* ptr = newObj.get();
				if (gameObjects_) {
					gameObjects_->push_back(std::move(newObj));
					selectedObject_ = ptr;
					isDirty_ = true;
					consoleMessages_.push_back("[Editor] Created: GameObject");
				}
			}
			if (ImGui::BeginMenu("Create Light")) {
				if (ImGui::MenuItem("Directional Light")) {
					auto newObj = std::make_unique<GameObject>();
					newObj->SetName("Directional Light");
					auto* dl = newObj->AddComponent<DirectionalLightComponent>();
					dl->UseTransformDirection(true);
					// Default rotation: angled down (45 deg around X)
					constexpr float DEG_TO_RAD = 0.0174532925f;
					newObj->GetTransform().SetLocalRotation(Quaternion::RotationRollPitchYaw(50.0f * DEG_TO_RAD, -30.0f * DEG_TO_RAD, 0.0f));
					auto* ptr = newObj.get();
					if (gameObjects_) {
						gameObjects_->push_back(std::move(newObj));
						selectedObject_ = ptr;
						FocusOnNewObject(ptr);
						isDirty_ = true;
						consoleMessages_.push_back("[Editor] Created: Directional Light");
					}
				}
				if (ImGui::MenuItem("Point Light")) {
					auto newObj = std::make_unique<GameObject>();
					newObj->SetName("Point Light");
					newObj->AddComponent<PointLightComponent>();
					newObj->GetTransform().SetLocalPosition(Vector3(0.0f, 3.0f, 0.0f));
					auto* ptr = newObj.get();
					if (gameObjects_) {
						gameObjects_->push_back(std::move(newObj));
						selectedObject_ = ptr;
						FocusOnNewObject(ptr);
						isDirty_ = true;
						consoleMessages_.push_back("[Editor] Created: Point Light");
					}
				}
				if (ImGui::MenuItem("Spot Light")) {
					auto newObj = std::make_unique<GameObject>();
					newObj->SetName("Spot Light");
					newObj->AddComponent<SpotLightComponent>();
					newObj->GetTransform().SetLocalPosition(Vector3(0.0f, 3.0f, 0.0f));
					// Point downward by default
					constexpr float DEG_TO_RAD = 0.0174532925f;
					newObj->GetTransform().SetLocalRotation(Quaternion::RotationRollPitchYaw(90.0f * DEG_TO_RAD, 0.0f, 0.0f));
					auto* ptr = newObj.get();
					if (gameObjects_) {
						gameObjects_->push_back(std::move(newObj));
						selectedObject_ = ptr;
						FocusOnNewObject(ptr);
						isDirty_ = true;
						consoleMessages_.push_back("[Editor] Created: Spot Light");
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
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


void EditorUI::PreLoadPendingThumbnails() {
    if (thumbnailRenderer_.IsInitialized() && thumbnailRenderer_.HasPending()) {
        thumbnailRenderer_.PreLoadPending();
    }
}
	void EditorUI::ProcessPendingThumbnails() {
		if (renderer_ && lightManager_ && thumbnailRenderer_.HasPending()) {
			thumbnailRenderer_.ProcessOne(renderer_, lightManager_, graphics_);
		}
	}

	void EditorUI::RenderProject(const EditorContext& context) {
		if (!showProject_) return;

		ImGui::Begin(U8("プロジェクト"), &showProject_);

		ImGui::Text(U8("アセット"));
		ImGui::Separator();

		// モデルフォルダをスキャン
		if (ImGui::TreeNodeEx(U8("モデル"), ImGuiTreeNodeFlags_DefaultOpen)) {
			// 遅延初期化
			if (!thumbnailRenderer_.IsInitialized() && graphics_ && resourceManager_) {
				thumbnailRenderer_.Initialize(graphics_, resourceManager_);
			}

			// 非同期スキャン完了チェック（毎フレーム）
			if (isModelScanning_ && modelScanFuture_.valid()) {
				if (modelScanFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
					cachedModelPaths_ = modelScanFuture_.get();
					isModelScanning_ = false;
				}
			}

			// 初回またはリフレッシュ時にスキャン開始（非同期）
			if (cachedModelPaths_.empty() && !isModelScanning_) {
				RefreshModelPaths();
			}

			// ツールバー
			{
				ImGui::PushStyleColor(ImGuiCol_Button,
					projectGridMode_ ? ImVec4(0.30f, 0.50f, 0.80f, 1.0f) : ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
				if (ImGui::SmallButton(U8(" ⊞ "))) projectGridMode_ = true;
				ImGui::PopStyleColor();

				ImGui::SameLine(0, 2);

				ImGui::PushStyleColor(ImGuiCol_Button,
					!projectGridMode_ ? ImVec4(0.30f, 0.50f, 0.80f, 1.0f) : ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
				if (ImGui::SmallButton(U8(" ≡ "))) projectGridMode_ = false;
				ImGui::PopStyleColor();

				ImGui::SameLine(0, 8);
				if (projectGridMode_) {
					ImGui::SetNextItemWidth(80.0f);
					ImGui::SliderFloat("##ThumbSize", &projectThumbnailSize_, 48.0f, 128.0f, "%.0f");
					ImGui::SameLine(0, 8);
				}

				if (ImGui::SmallButton(U8("更新"))) {
					RefreshModelPaths();
					projectSelectedModelIdx_ = -1;
					consoleMessages_.push_back(U8("[エディタ] モデルリストを更新しました"));
				}
			}
			ImGui::Separator();

			if (isModelScanning_) {
				ImGui::TextDisabled(U8("アセットをスキャン中..."));
			} else if (cachedModelPaths_.empty()) {
				ImGui::TextDisabled(U8("(モデルなし)"));
			} else if (projectGridMode_) {
				// ── グリッドビュー ──
				const float pad   = 6.0f;
				const float sz    = projectThumbnailSize_;
				const float nameH = ImGui::GetTextLineHeightWithSpacing();
				const float cellH = sz + nameH;
				const float avail = ImGui::GetContentRegionAvail().x;
				const int   cols  = std::max(1, static_cast<int>((avail + pad) / (sz + pad)));

				// スクロール可能領域（プレビューパネル分を残す）
				float scrollH = 0.0f;
				ImGui::BeginChild("##ModelGrid", ImVec2(0, scrollH), false);

				int col = 0;
				for (size_t i = 0; i < cachedModelPaths_.size(); ++i) {
					const auto& modelPath = cachedModelPaths_[i];
					std::filesystem::path p(modelPath);
					std::string ext      = p.extension().string();
					std::string filename = p.filename().string();

					if (ext == ".obj") continue;

					if (col > 0 && col % cols != 0) ImGui::SameLine(0, pad);

					ImGui::PushID(static_cast<int>(i));

					ImVec2 tilePos = ImGui::GetCursorScreenPos();
					bool   sel     = (projectSelectedModelIdx_ == static_cast<int>(i));
					auto*  dl      = ImGui::GetWindowDrawList();

					// 背景
					ImU32 bgCol = sel ? IM_COL32(70, 55, 10, 220) : IM_COL32(50, 50, 50, 200);
					dl->AddRectFilled(tilePos, { tilePos.x + sz, tilePos.y + cellH }, bgCol, 4.0f);

					// サムネイル or プレースホルダー
					D3D12_GPU_DESCRIPTOR_HANDLE thumb{ 0 };
					if (thumbnailRenderer_.IsInitialized()) {
						thumb = thumbnailRenderer_.Request(modelPath);
					}
					if (thumb.ptr != 0) {
						dl->AddImage(
							(ImTextureID)thumb.ptr,
							tilePos, { tilePos.x + sz, tilePos.y + sz }
						);
					} else {
						dl->AddRectFilled(tilePos, { tilePos.x + sz, tilePos.y + sz }, IM_COL32(35, 35, 35, 255), 3.0f);
						// 読み込み中アイコン（中央）
						ImVec2 ic = { tilePos.x + sz * 0.5f - 8, tilePos.y + sz * 0.5f - 7 };
						dl->AddText(ic, IM_COL32(140, 140, 140, 255), "...");
					}

					// ファイル名（中央揃え・トリミング）
					std::string shortName = filename;
					if (shortName.size() > 12) shortName = shortName.substr(0, 11) + "~";
					float tw = ImGui::CalcTextSize(shortName.c_str()).x;
					dl->AddText(
						{ tilePos.x + (sz - tw) * 0.5f, tilePos.y + sz + 2 },
						IM_COL32(220, 220, 220, 255), shortName.c_str()
					);

					// クリック / D&D / ダブルクリック
					ImGui::InvisibleButton("##tile", { sz, cellH });

					if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
						projectSelectedModelIdx_ = static_cast<int>(i);
					}
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
						HandleModelDragDropByIndex(i);
					}
					if (ImGui::IsItemHovered()) {
						dl->AddRectFilled(tilePos, { tilePos.x + sz, tilePos.y + cellH }, IM_COL32(255, 255, 255, 18), 4.0f);
					}
					if (sel) {
						dl->AddRect(tilePos, { tilePos.x + sz, tilePos.y + cellH }, IM_COL32(255, 200, 0, 255), 4.0f, 0, 2.0f);
					}
					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
						ImGui::SetDragDropPayload("MODEL_INDEX", &i, sizeof(size_t));
						ImGui::Text("%s", filename.c_str());
						ImGui::EndDragDropSource();
					}

					ImGui::PopID();
					col++;
				}

				ImGui::EndChild();
			} else {
				// ── リストビュー（従来） ──
				for (size_t i = 0; i < cachedModelPaths_.size(); ++i) {
					const auto& modelPath = cachedModelPaths_[i];
					std::filesystem::path p(modelPath);
					std::string ext      = p.extension().string();
					std::string filename = p.filename().string();

					if (ext == ".obj") continue;

					ImGui::PushID(static_cast<int>(i));

					const char* icon = (ext == ".gltf" || ext == ".glb") ? "🎨" : "📦";
					ImGui::Text("%s", icon);
					ImGui::SameLine();

					bool sel = (projectSelectedModelIdx_ == static_cast<int>(i));
					if (ImGui::Selectable(filename.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick)) {
						projectSelectedModelIdx_ = static_cast<int>(i);
						if (ImGui::IsMouseDoubleClicked(0)) {
							HandleModelDragDropByIndex(i);
						}
					}
					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
						ImGui::SetDragDropPayload("MODEL_INDEX", &i, sizeof(size_t));
						ImGui::Text("Drag: %s", filename.c_str());
						ImGui::EndDragDropSource();
					}

					ImGui::PopID();
				}
			}

			// サムネイル生成進捗
			{
				size_t pending = thumbnailRenderer_.GetPendingCount();
				size_t total   = thumbnailRenderer_.GetTotalCount();
				if (total > 0 && pending > 0) {
					size_t done = total - pending;
					float prog  = static_cast<float>(done) / static_cast<float>(total);
					ImGui::Separator();
					ImGui::TextDisabled(U8("サムネイル生成中... (%zu / %zu)"), done, total);
					ImGui::ProgressBar(prog, {-1.0f, 6.0f}, "");
				}
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
						PushExecutedCommand(std::make_unique<CreateObjectCommand>(gameObjects_, selectedObject_, &selectedObject_, &expandedObjects_));
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

		// ビデオフォルダをスキャン
		if (ImGui::TreeNode(U8("ビデオ"))) {
			if (ImGui::SmallButton(U8("更新##Video"))) {
				RefreshVideoPaths();
				consoleMessages_.push_back(U8("[エディタ] ビデオリストを更新しました"));
			}
			ImGui::Separator();

			if (cachedVideoPaths_.empty()) {
				RefreshVideoPaths();
			}

			for (size_t i = 0; i < cachedVideoPaths_.size(); ++i) {
				const auto& videoPath = cachedVideoPaths_[i];
				std::filesystem::path p(videoPath);
				std::string filename = p.filename().string();

				ImGui::PushID(static_cast<int>(i + 30000));

				ImGui::Text("🎬");
				ImGui::SameLine();

				if (ImGui::Selectable(filename.c_str())) {
					// シングルクリック: VideoPlayerComponentがある選択中オブジェクトにセット
					if (selectedObject_) {
						if (auto* videoPlayer = selectedObject_->GetComponent<VideoPlayerComponent>()) {
							videoPlayer->LoadVideo(videoPath);
							consoleMessages_.push_back("[Editor] Video set: " + filename);
						}
					}
				}

				// ダブルクリック: 新規GameObjectを作成してVideoPlayerComponentを追加
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
					if (gameObjects_ && graphics_) {
						std::string objectName = p.stem().string();
						auto newObject = std::make_unique<GameObject>(objectName);
						auto* videoPlayer = newObject->AddComponent<VideoPlayerComponent>();
						videoPlayer->SetGraphicsDevice(graphics_);
						videoPlayer->LoadVideo(videoPath);
						selectedObject_ = newObject.get();
						gameObjects_->push_back(std::move(newObject));
						PushExecutedCommand(std::make_unique<CreateObjectCommand>(gameObjects_, selectedObject_, &selectedObject_, &expandedObjects_));
						consoleMessages_.push_back("[Editor] Created VideoPlayer object: " + objectName);
					}
				}

				// ドラッグ＆ドロップソース
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
					ImGui::SetDragDropPayload("VIDEO_PATH", videoPath.c_str(), videoPath.size() + 1);
					ImGui::Text("🎬 %s", filename.c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::PopID();
			}

			if (cachedVideoPaths_.empty()) {
				ImGui::TextDisabled(U8("(ビデオファイルなし)"));
			}

			ImGui::TreePop();
		}

		// ライト作成セクション
		if (ImGui::TreeNode(U8("ライト"))) {
			struct LightPreset {
				const char* icon;
				const char* name;
				int type; // 0=Directional, 1=Point, 2=Spot
			};
			LightPreset presets[] = {
				{"☀", "Directional Light", 0},
				{"💡", "Point Light", 1},
				{"🔦", "Spot Light", 2},
			};

			for (int li = 0; li < 3; ++li) {
				auto& preset = presets[li];
				ImGui::PushID(li + 50000);
				ImGui::Text("%s", preset.icon);
				ImGui::SameLine();
				if (ImGui::Selectable(preset.name, false, ImGuiSelectableFlags_AllowDoubleClick)) {
					if (ImGui::IsMouseDoubleClicked(0) && gameObjects_) {
						auto newObj = std::make_unique<GameObject>();
						newObj->SetName(preset.name);
						constexpr float DEG_TO_RAD = 0.0174532925f;
						if (preset.type == 0) {
							auto* dl = newObj->AddComponent<DirectionalLightComponent>();
							dl->UseTransformDirection(true);
							newObj->GetTransform().SetLocalRotation(
								Quaternion::RotationRollPitchYaw(50.0f * DEG_TO_RAD, -30.0f * DEG_TO_RAD, 0.0f));
						} else if (preset.type == 1) {
							newObj->AddComponent<PointLightComponent>();
							newObj->GetTransform().SetLocalPosition(Vector3(0.0f, 3.0f, 0.0f));
						} else {
							newObj->AddComponent<SpotLightComponent>();
							newObj->GetTransform().SetLocalPosition(Vector3(0.0f, 3.0f, 0.0f));
							newObj->GetTransform().SetLocalRotation(
								Quaternion::RotationRollPitchYaw(90.0f * DEG_TO_RAD, 0.0f, 0.0f));
						}
						selectedObject_ = newObj.get();
						gameObjects_->push_back(std::move(newObj));
						FocusOnNewObject(selectedObject_);
						isDirty_ = true;
						consoleMessages_.push_back(std::string("[Editor] Created: ") + preset.name);
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(U8("ダブルクリックでシーンに配置"));
				}
				ImGui::PopID();
			}

			ImGui::TreePop();
		}

		// プロシージャルジェネレーター
		if (ImGui::TreeNode(U8("ジェネレーター"))) {
			ImGui::PushID(60000);

			// 草原
			{
				bool isSelected = (selectedGenerator_ == GeneratorType::Grass);
				if (isSelected) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
				}
				ImGui::Text("🌿");
				ImGui::SameLine();
				if (ImGui::Selectable(U8("草原"), isSelected)) {
					if (isSelected) {
						// 選択解除
						selectedGenerator_ = GeneratorType::None;
						grassPaintActive_ = false;
						consoleMessages_.push_back(U8("[ジェネレーター] 草原の選択を解除"));
					} else {
						// 選択 → ペイントON + テクスチャモード
						selectedGenerator_ = GeneratorType::Grass;
						selectedObject_ = nullptr;
						grassPaintActive_ = true;
						inspectorTabIndex_ = 2;
						if (auto* gr = renderer_ ? renderer_->GetGrassRenderer() : nullptr) {
							gr->SetUseGodotShading(false);
						}
						consoleMessages_.push_back(U8("[ジェネレーター] 草原ペイント ON - テクスチャベース"));
					}
				}
				if (isSelected) {
					ImGui::PopStyleColor();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(U8("テクスチャベースの草原ペイント\nScene Viewでマウスドラッグで配置"));
				}
			}

			// GodotGrass
			{
				bool isSelected = (selectedGenerator_ == GeneratorType::GodotGrass);
				if (isSelected) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.6f, 1.0f));
				}
				ImGui::Text("🌾");
				ImGui::SameLine();
				if (ImGui::Selectable("GodotGrass", isSelected)) {
					if (isSelected) {
						selectedGenerator_ = GeneratorType::None;
						grassPaintActive_ = false;
						consoleMessages_.push_back(U8("[ジェネレーター] GodotGrass の選択を解除"));
					} else {
						selectedGenerator_ = GeneratorType::GodotGrass;
						selectedObject_ = nullptr;
						grassPaintActive_ = true;
						inspectorTabIndex_ = 2;
						if (auto* gr = renderer_ ? renderer_->GetGrassRenderer() : nullptr) {
							gr->SetUseGodotShading(true);
						}
						consoleMessages_.push_back(U8("[ジェネレーター] GodotGrass ペイント ON - Shaderベース草原"));
					}
				}
				if (isSelected) {
					ImGui::PopStyleColor();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(U8("Godot風シェーダー草原\nグラデーション+ノイズ風+踏み倒し+風影"));
				}
			}

			ImGui::PopID();
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
				ClearSelection();
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

		// Ctrl+Shift+C: シネマティックエディタ トグル
		if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
			cinematicEditor_.ToggleOpen();
			if (cinematicEditor_.IsOpen()) {
				cinematicEditor_.SetSceneViewCamera(&sceneViewCamera_);
				cinematicEditor_.SetPreviewCamera(&sceneViewCamera_);
			}
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

		// Ctrl+Y: Redo
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			PerformRedo();
		}

		// Ctrl+S: シーン保存
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			SaveScene("assets/scenes/default_scene.json");
		}

		// Ctrl+C: 選択オブジェクトをクリップボードにコピー
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false) && editorMode_ == EditorMode::Edit) {
			clipboard_.clear();
			if (!selectedObjects_.empty()) {
				for (auto* obj : selectedObjects_) {
					if (obj) clipboard_.push_back(SceneSerializer::SerializeSingleObject(*obj));
				}
			} else if (selectedObject_) {
				clipboard_.push_back(SceneSerializer::SerializeSingleObject(*selectedObject_));
			}
			if (!clipboard_.empty()) {
				consoleMessages_.push_back("[Editor] Copied " + std::to_string(clipboard_.size()) + " object(s)");
			}
		}

		// Ctrl+V: クリップボードからペースト
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && editorMode_ == EditorMode::Edit) {
			if (!clipboard_.empty() && gameObjects_) {
				auto batch = std::make_unique<BatchCommand>();
				selectedObjects_.clear();
				for (const auto& json : clipboard_) {
					auto newObj = SceneSerializer::DeserializeSingleObject(json);
					if (newObj) {
						newObj->SetName(newObj->GetName() + " (Copy)");
						auto& t = newObj->GetTransform();
						auto pos = t.GetLocalPosition();
						t.SetLocalPosition(pos + Vector3(1.0f, 0.0f, 1.0f));
						GameObject* rawPtr = newObj.get();
						gameObjects_->push_back(std::move(newObj));
						if (scene_) scene_->StartGameObject(rawPtr);

						// リソースロードは遅延処理（レンダリング中のコマンドリスト衝突を回避）
						pendingPasteResourceLoads_.push_back(rawPtr);

						selectedObjects_.insert(rawPtr);
						selectedObject_ = rawPtr;

						auto cmd = std::make_unique<CreateObjectCommand>(gameObjects_, rawPtr, &selectedObject_, &expandedObjects_);
						batch->commands.push_back(std::move(cmd));
					}
				}
				if (!batch->commands.empty()) {
					consoleMessages_.push_back("[Editor] Pasted " + std::to_string(batch->commands.size()) + " object(s)");
					// Already executed (objects added above), just push to undo history
					PushExecutedCommand(std::move(batch));
				}
			}
		}

		// DEL: 選択オブジェクト削除（どのウィンドウにフォーカスがあっても動作）
		if (!renamingObject_ && editorMode_ == EditorMode::Edit
			&& ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
			// マルチセレクション対応
			if (selectedObjects_.size() > 1 && gameObjects_) {
				auto batch = std::make_unique<BatchCommand>();
				for (auto* obj : selectedObjects_) {
					if (obj && obj->IsDeletable()) {
						consoleMessages_.push_back("[Editor] Deleted: " + obj->GetName());
						auto cmd = std::make_unique<DeleteObjectCommand>(gameObjects_, obj, &selectedObject_, &expandedObjects_);
						batch->commands.push_back(std::move(cmd));
					}
				}
				if (!batch->commands.empty()) {
					ExecuteCommand(std::move(batch));
				}
				selectedObjects_.clear();
			} else if (selectedObject_) {
				if (!selectedObject_->IsDeletable()) {
					consoleMessages_.push_back("[Editor] Cannot delete: " + selectedObject_->GetName() + " (protected)");
				} else if (gameObjects_) {
					consoleMessages_.push_back("[Editor] Deleted: " + selectedObject_->GetName());
					ExecuteCommand(std::make_unique<DeleteObjectCommand>(gameObjects_, selectedObject_, &selectedObject_, &expandedObjects_));
					selectedObjects_.clear();
				}
			}
		}
	}

	// Undo履歴に追加
	void EditorUI::ExecuteCommand(std::unique_ptr<IEditorCommand> cmd) {
		cmd->Execute();
		undoHistory_.push_back(std::move(cmd));
		if (undoHistory_.size() > kMaxUndoHistory) undoHistory_.pop_front();
		redoHistory_.clear();
		isDirty_ = true;
	}

	void EditorUI::PushExecutedCommand(std::unique_ptr<IEditorCommand> cmd) {
		undoHistory_.push_back(std::move(cmd));
		if (undoHistory_.size() > kMaxUndoHistory) undoHistory_.pop_front();
		redoHistory_.clear();
		isDirty_ = true;
	}

	// Undo実行
	void EditorUI::PerformUndo() {
		if (undoHistory_.empty()) {
			consoleMessages_.push_back(U8("[エディタ] 元に戻す操作がありません"));
			return;
		}
		auto cmd = std::move(undoHistory_.back());
		undoHistory_.pop_back();
		cmd->Undo();
		redoHistory_.push_back(std::move(cmd));
		if (redoHistory_.size() > kMaxUndoHistory) redoHistory_.pop_front();
		consoleMessages_.push_back(U8("[エディタ] 元に戻しました"));
		isDirty_ = true;
	}

	void EditorUI::PerformRedo() {
		if (redoHistory_.empty()) {
			consoleMessages_.push_back(U8("[エディタ] やり直す操作がありません"));
			return;
		}
		auto cmd = std::move(redoHistory_.back());
		redoHistory_.pop_back();
		cmd->Execute();
		undoHistory_.push_back(std::move(cmd));
		if (undoHistory_.size() > kMaxUndoHistory) undoHistory_.pop_front();
		consoleMessages_.push_back(U8("[エディタ] やり直しました"));
		isDirty_ = true;
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
		isInspectorEditing_ = false;

		auto* obj = preInspectorSnapshot_.targetObject;
		if (!obj) return;
		auto& t = obj->GetTransform();
		auto cmd = std::make_unique<TransformCommand>();
		cmd->object   = obj;
		cmd->oldPos   = preInspectorSnapshot_.position;
		cmd->oldRot   = preInspectorSnapshot_.rotation;
		cmd->oldScale = preInspectorSnapshot_.scale;
		cmd->newPos   = t.GetLocalPosition();
		cmd->newRot   = t.GetLocalRotation();
		cmd->newScale = t.GetLocalScale();
		PushExecutedCommand(std::move(cmd));
	}

	// シーン保存
	void EditorUI::SaveScene(const std::string& filepath) {
		if (!gameObjects_) {
			consoleMessages_.push_back(U8("[エディタ] エラー: 保存するオブジェクトがありません"));
			return;
		}

		// 保存前にPostProcessManagerの現在値をCameraComponentに同期
		SyncPostProcessParamsToCamera();

		GrassSystem* grassSystem = renderer_ ? renderer_->GetGrassSystem() : nullptr;
		if (SceneSerializer::SaveScene(*gameObjects_, filepath, grassSystem)) {
			consoleMessages_.push_back(U8("[エディタ] シーンを保存しました: ") + filepath);
			// EditorCameraの設定も保存
			editorCamera_.SaveSettings();
			consoleMessages_.push_back(U8("[エディタ] カメラ設定を保存しました"));

			// NavMesh情報を表示
			auto& navMesh = Navigation::NavMeshManager::Get();
			if (navMesh.IsBuilt()) {
				consoleMessages_.push_back(U8("[エディタ] NavMesh設定とデータを保存しました"));
			} else {
				consoleMessages_.push_back(U8("[エディタ] NavMesh設定を保存しました（ベイクデータなし）"));
			}

			// 草原情報を表示
			if (grassSystem && grassSystem->GetInstanceCount() > 0) {
				consoleMessages_.push_back(U8("[エディタ] 草原データを保存しました (") +
					std::to_string(grassSystem->GetInstanceCount()) + U8("本)"));
			}

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

		GrassSystem* grassSystem = renderer_ ? renderer_->GetGrassSystem() : nullptr;
		if (SceneSerializer::LoadScene(filepath, *gameObjects_, grassSystem)) {
			consoleMessages_.push_back(U8("[エディタ] シーンを読み込みました: ") + filepath);
			// ロード後、最初のオブジェクトを選択
			if (!gameObjects_->empty()) {
				selectedObject_ = (*gameObjects_)[0].get();
			}
			// CameraComponentのPostProcessパラメータをPostProcessManagerに同期
			SyncPostProcessParamsFromCamera();

			// NavMesh読み込み状態を表示
			auto& navMesh = Navigation::NavMeshManager::Get();
			if (navMesh.IsBuilt()) {
				consoleMessages_.push_back(U8("[エディタ] NavMeshを読み込みました"));
				showRecastNavMesh_ = true;  // 自動的にNavMesh表示をON
			}

			// 草原読み込み状態を表示
			if (grassSystem && grassSystem->GetInstanceCount() > 0) {
				consoleMessages_.push_back(U8("[エディタ] 草原データを読み込みました (") +
					std::to_string(grassSystem->GetInstanceCount()) + U8("本)"));
			}
			// 草テクスチャパスが保存されていたら復元
			if (grassSystem && !grassSystem->GetGrassTexturePath().empty()) {
				auto* grassRenderer = renderer_->GetGrassRenderer();
				if (grassRenderer && graphics_) {
					graphics_->BeginResourceUpload();
					if (grassRenderer->LoadTextureFromFile(grassSystem->GetGrassTexturePath())) {
						consoleMessages_.push_back(U8("[エディタ] 草テクスチャを復元: ") +
							grassSystem->GetGrassTexturePath());
					}
					graphics_->EndResourceUpload();
				}
			}
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
			// PS1パラメータを同期
			if (auto* ps1 = postProcessManager_->GetPS1()) {
				camComp->SetPS1Params(ps1->GetParams());
			}
			// ChromaticAberrationパラメータを同期
			if (auto* chromatic = postProcessManager_->GetChromaticAberration()) {
				camComp->SetChromaticAberrationParams(chromatic->GetParams());
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
			// PS1パラメータを同期
			if (auto* ps1 = postProcessManager_->GetPS1()) {
				ps1->SetParams(camComp->GetPS1Params());
			}
			// ChromaticAberrationパラメータを同期
			if (auto* chromatic = postProcessManager_->GetChromaticAberration()) {
				chromatic->SetParams(camComp->GetChromaticAberrationParams());
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

	void EditorUI::RefreshAssetPaths(std::vector<std::string>& cache,
	                                  std::string_view directory,
	                                  std::span<const std::string_view> extensions) {
		cache.clear();
		std::filesystem::path dirPath(directory);

		if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath))
			return;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
			if (!entry.is_regular_file())
				continue;

			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

			for (auto validExt : extensions) {
				if (ext == validExt) {
					std::string relativePath = entry.path().string();
					std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
					cache.push_back(relativePath);
					break;
				}
			}
		}
	}

	void EditorUI::RefreshModelPaths() {
		if (isModelScanning_) return;
		isModelScanning_ = true;
		cachedModelPaths_.clear();

		modelScanFuture_ = std::async(std::launch::async, []() -> std::vector<std::string> {
			std::vector<std::string> result;
			constexpr std::string_view exts[] = { ".gltf", ".glb", ".fbx", ".obj" };

			std::filesystem::path dirPath("assets/model");
			if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath))
				return result;

			for (const auto& entry : std::filesystem::recursive_directory_iterator(
					dirPath, std::filesystem::directory_options::skip_permission_denied)) {
				if (!entry.is_regular_file()) continue;

				std::string ext = entry.path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				for (auto validExt : exts) {
					if (ext == validExt) {
						std::string relativePath = entry.path().string();
						std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
						result.push_back(relativePath);
						break;
					}
				}
			}
			return result;
		});
	}

	void EditorUI::QueueAllThumbnails() {
		// ThumbnailRendererを初期化
		if (!thumbnailRenderer_.IsInitialized() && graphics_ && resourceManager_) {
			thumbnailRenderer_.Initialize(graphics_, resourceManager_);
		}
		if (!thumbnailRenderer_.IsInitialized()) return;

		// モデルパスを同期スキャン
		constexpr std::string_view exts[] = { ".gltf", ".glb", ".fbx", ".obj" };
		std::filesystem::path dirPath("assets/model");
		if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath))
			return;

		cachedModelPaths_.clear();
		for (const auto& entry : std::filesystem::recursive_directory_iterator(
				dirPath, std::filesystem::directory_options::skip_permission_denied)) {
			if (!entry.is_regular_file()) continue;

			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

			for (auto validExt : exts) {
				if (ext == validExt) {
					std::string relativePath = entry.path().string();
					std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
					cachedModelPaths_.push_back(relativePath);
					break;
				}
			}
		}

		// .objを除外してサムネイルをキューに積む
		for (const auto& path : cachedModelPaths_) {
			std::filesystem::path p(path);
			if (p.extension() == ".obj") continue;
			thumbnailRenderer_.Request(path);
		}
	}

	void EditorUI::RefreshAudioPaths() {
		constexpr std::string_view exts[] = { ".wav" };
		RefreshAssetPaths(cachedAudioPaths_, "assets/audio", exts);
	}

	void EditorUI::RefreshVideoPaths() {
		constexpr std::string_view exts[] = { ".mp4", ".avi", ".mkv", ".webm", ".mov" };
		RefreshAssetPaths(cachedVideoPaths_, "assets/video", exts);
	}

	void EditorUI::RefreshScriptPaths() {
		constexpr std::string_view exts[] = { ".lua" };
		RefreshAssetPaths(cachedScriptPaths_, "assets/scripts", exts);
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
		// ペーストされたオブジェクトのリソースロード
		if (!pendingPasteResourceLoads_.empty() && resourceManager_) {
			for (auto* obj : pendingPasteResourceLoads_) {
				if (!obj) continue;

				auto* skinnedRenderer = obj->GetComponent<SkinnedMeshRenderer>();
				if (skinnedRenderer) {
					std::string modelPath = skinnedRenderer->GetModelPath();
					if (!modelPath.empty()) {
						resourceManager_->BeginUpload();
						auto* modelData = resourceManager_->LoadSkinnedModel(modelPath);
						resourceManager_->EndUpload();
						if (modelData) {
							skinnedRenderer->SetModel(modelData);
							auto* animator = obj->GetComponent<AnimatorComponent>();
							if (!animator) animator = obj->AddComponent<AnimatorComponent>();
							if (modelData->skeleton) {
								animator->Initialize(modelData->skeleton, modelData->animations);
								if (!modelData->animations.empty()) {
									animator->Play(modelData->animations[0]->GetName(), true);
								}
							}
						}
					}
				}

				auto* meshRenderer = obj->GetComponent<MeshRenderer>();
				if (meshRenderer) {
					std::string modelPath = meshRenderer->GetModelPath();
					if (!modelPath.empty()) {
						resourceManager_->BeginUpload();
						auto* modelData = resourceManager_->LoadStaticModel(modelPath);
						resourceManager_->EndUpload();
						if (modelData && !modelData->meshes.empty()) {
							meshRenderer->SetModel(modelData);
							auto* collision = obj->GetComponent<CollisionComponent>();
							if (collision) collision->RecalculateFromMesh();
							auto* meshCollider = obj->GetComponent<MeshColliderComponent>();
							if (meshCollider) meshCollider->RebuildBVH();
						}
					}
				}
			}
			pendingPasteResourceLoads_.clear();
		}

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
				renderer->SetModel(staticModelData);  // 全メッシュを含むモデルデータを設定
				consoleMessages_.push_back("[Editor] Loaded as static model (" + std::to_string(staticModelData->meshes.size()) + " meshes)");
			}

			// 選択状態にする
			selectedObject_ = newObject.get();

			// GameObjectsリストに追加
			gameObjects_->push_back(std::move(newObject));
			PushExecutedCommand(std::make_unique<CreateObjectCommand>(gameObjects_, selectedObject_, &selectedObject_, &expandedObjects_));
			isDirty_ = true;

			// 重要: コンポーネントのStart()を呼んで初期化
			// （再起動時はScene::ProcessPendingStarts()で呼ばれるが、D&D時は手動で呼ぶ必要がある）
			if (scene_) {
				scene_->StartGameObject(selectedObject_);
			}

			// Scene ViewへのD&Dならドロップ位置に配置、それ以外はカメラフォーカス
			if (pendingDropPosition_.has_value()) {
				selectedObject_->GetTransform().SetLocalPosition(pendingDropPosition_.value());
			} else {
				FocusOnNewObject(selectedObject_);
			}

			consoleMessages_.push_back("[Editor] Created object: " + modelName);
	}

	// キューをクリア
		pendingModelLoads_.clear();
		pendingDropPosition_.reset();
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
		if (!debugRenderer) {
			return;
		}

		// グリッド表示フラグを同期（常に実行）
		debugRenderer->SetShowGrid(showGrid_);

		// GameObjectsがない、またはScene View非表示なら以降の処理をスキップ
		if (!gameObjects_ || !showSceneView_) {
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

			// Frustum表示が有効な場合のみ描画
			if (showCameraFrustum_) {
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

		// ライトギズモの描画
		for (const auto& obj : *gameObjects_) {
			bool isSelected = (selectedObject_ == obj.get());

			// Directional Light: 球アイコン + 方向矢印
			if (auto* dl = obj->GetComponent<DirectionalLightComponent>()) {
				Vector3 pos = obj->GetTransform().GetLocalPosition();
				Vector3 dir = dl->GetDirection();
				Vector3 col = dl->GetColor();
				Vector4 gizmoColor = isSelected
					? Vector4(1.0f, 1.0f, 0.0f, 1.0f)
					: Vector4(col.GetX(), col.GetY(), col.GetZ(), 1.0f);

				debugRenderer->AddSphere(pos, 0.3f, gizmoColor, 8);
				// Direction arrow (3 lines from center)
				float arrowLen = 2.0f;
				debugRenderer->AddLine(pos, pos + dir * arrowLen, gizmoColor);
				// Arrow head lines
				Vector3 arrowEnd = pos + dir * arrowLen;
				Vector3 perpA = dir.Cross(Vector3(0, 1, 0));
				if (perpA.Length() < 0.001f) perpA = dir.Cross(Vector3(1, 0, 0));
				perpA = perpA.Normalize() * 0.3f;
				Vector3 perpB = dir.Cross(perpA).Normalize() * 0.3f;
				debugRenderer->AddLine(arrowEnd, arrowEnd - dir * 0.5f + perpA, gizmoColor);
				debugRenderer->AddLine(arrowEnd, arrowEnd - dir * 0.5f - perpA, gizmoColor);
				debugRenderer->AddLine(arrowEnd, arrowEnd - dir * 0.5f + perpB, gizmoColor);
				debugRenderer->AddLine(arrowEnd, arrowEnd - dir * 0.5f - perpB, gizmoColor);
			}

			// Point Light: 球 + range sphere
			if (auto* pl = obj->GetComponent<PointLightComponent>()) {
				Vector3 pos = obj->GetTransform().GetLocalPosition();
				Vector3 col = pl->GetColor();
				Vector4 gizmoColor = isSelected
					? Vector4(1.0f, 1.0f, 0.0f, 1.0f)
					: Vector4(col.GetX(), col.GetY(), col.GetZ(), 1.0f);

				debugRenderer->AddSphere(pos, 0.2f, gizmoColor, 8);
				// Range sphere (wireframe)
				Vector4 rangeColor(gizmoColor.GetX(), gizmoColor.GetY(), gizmoColor.GetZ(), 0.4f);
				debugRenderer->AddSphere(pos, pl->GetRange(), rangeColor, 16);
			}

			// Spot Light: 球 + cone lines
			if (auto* sl = obj->GetComponent<SpotLightComponent>()) {
				Vector3 pos = obj->GetTransform().GetLocalPosition();
				Vector3 dir = -obj->GetTransform().GetForward();  // Same as BuildGPULightData
				Vector3 col = sl->GetColor();
				Vector4 gizmoColor = isSelected
					? Vector4(1.0f, 1.0f, 0.0f, 1.0f)
					: Vector4(col.GetX(), col.GetY(), col.GetZ(), 1.0f);

				debugRenderer->AddSphere(pos, 0.2f, gizmoColor, 8);

				// Cone visualization (8 lines)
				float range = sl->GetRange();
				float halfAngle = sl->GetSpotAngle() * 0.0174532925f;  // deg to rad
				float coneRadius = range * std::tan(halfAngle);

				Vector3 perpX = dir.Cross(Vector3(0, 1, 0));
				if (perpX.Length() < 0.001f) perpX = dir.Cross(Vector3(1, 0, 0));
				perpX = perpX.Normalize();
				Vector3 perpY = dir.Cross(perpX).Normalize();

				Vector3 tipEnd = pos + dir * range;
				constexpr int coneLines = 8;
				constexpr float pi2 = 6.28318530718f;
				for (int i = 0; i < coneLines; ++i) {
					float angle = (static_cast<float>(i) / coneLines) * pi2;
					Vector3 rimPoint = tipEnd + perpX * (std::cos(angle) * coneRadius) + perpY * (std::sin(angle) * coneRadius);
					debugRenderer->AddLine(pos, rimPoint, gizmoColor);
				}
				// Draw rim circle
				for (int i = 0; i < coneLines; ++i) {
					float angle1 = (static_cast<float>(i) / coneLines) * pi2;
					float angle2 = (static_cast<float>(i + 1) / coneLines) * pi2;
					Vector3 p1 = tipEnd + perpX * (std::cos(angle1) * coneRadius) + perpY * (std::sin(angle1) * coneRadius);
					Vector3 p2 = tipEnd + perpX * (std::cos(angle2) * coneRadius) + perpY * (std::sin(angle2) * coneRadius);
					debugRenderer->AddLine(p1, p2, gizmoColor);
				}
			}
		}

		// Collision AABBの描画
		for (const auto& obj : *gameObjects_) {
			auto* collision = obj->GetComponent<CollisionComponent>();
			if (!collision || !collision->IsEnabled()) continue;

			AABB worldAABB = collision->GetWorldAABB();
			
			// 衝突中は赤、通常は緑
			Vector4 aabbColor = collision->IsColliding()
				? Vector4(1.0f, 0.0f, 0.0f, 1.0f)  // 赤（衝突中）
				: Vector4(0.0f, 1.0f, 0.0f, 1.0f); // 緑（通常）

			// 選択中のオブジェクトは黄色
			if (selectedObject_ == obj.get()) {
				aabbColor = Vector4(1.0f, 1.0f, 0.0f, 1.0f); // 黄色（選択中）
			}

			debugRenderer->AddBox(worldAABB.min, worldAABB.max, aabbColor);
		}

		// NavMeshデバッグ描画
		if (showRecastNavMesh_) {
			auto& recastNavMesh = Navigation::NavMeshManager::Get();
			if (recastNavMesh.IsBuilt()) {
				recastNavMesh.DebugDraw(debugRenderer);
			}
		}

		// NavAgent可視化（エージェント半径・高さを円柱で表示）
		auto& navMeshManager = Navigation::NavMeshManager::Get();
		auto navSettings = navMeshManager.GetSettings();
		float agentRadius = navSettings.agentRadius;
		float agentHeight = navSettings.agentHeight;

		for (const auto& obj : *gameObjects_) {
			auto* navAgent = obj->GetComponent<NavAgentComponent>();
			if (!navAgent) continue;

			const auto& transform = obj->GetTransform();
			Vector3 basePos = transform.GetPosition();

			// 色設定（選択中は黄色、通常はシアン）
			Vector4 agentColor = (selectedObject_ == obj.get())
				? Vector4(1.0f, 1.0f, 0.0f, 1.0f)  // 黄色（選択中）
				: Vector4(0.0f, 0.8f, 1.0f, 1.0f); // シアン（通常）

			// 円を描画（底面と上面）
			constexpr int segments = 24;
			constexpr float pi2 = 6.28318530718f;

			for (int i = 0; i < segments; ++i) {
				float angle1 = (static_cast<float>(i) / segments) * pi2;
				float angle2 = (static_cast<float>(i + 1) / segments) * pi2;

				float x1 = std::cos(angle1) * agentRadius;
				float z1 = std::sin(angle1) * agentRadius;
				float x2 = std::cos(angle2) * agentRadius;
				float z2 = std::sin(angle2) * agentRadius;

				// 底面の円
				Vector3 p1Bottom(basePos.GetX() + x1, basePos.GetY(), basePos.GetZ() + z1);
				Vector3 p2Bottom(basePos.GetX() + x2, basePos.GetY(), basePos.GetZ() + z2);
				debugRenderer->AddLine(p1Bottom, p2Bottom, agentColor);

				// 上面の円
				Vector3 p1Top(basePos.GetX() + x1, basePos.GetY() + agentHeight, basePos.GetZ() + z1);
				Vector3 p2Top(basePos.GetX() + x2, basePos.GetY() + agentHeight, basePos.GetZ() + z2);
				debugRenderer->AddLine(p1Top, p2Top, agentColor);

				// 縦線（4本）
				if (i % (segments / 4) == 0) {
					debugRenderer->AddLine(p1Bottom, p1Top, agentColor);
				}
			}

			// パス表示（移動中の場合）
			if (navAgent->HasPath() && navAgent->IsPathVisualized()) {
				const auto& path = navAgent->GetCurrentPath();
				Vector4 pathColor(0.0f, 1.0f, 0.5f, 1.0f); // 緑
				for (size_t i = 0; i + 1 < path.size(); ++i) {
					Vector3 from(path[i].x, path[i].y + 0.1f, path[i].z);
					Vector3 to(path[i + 1].x, path[i + 1].y + 0.1f, path[i + 1].z);
					debugRenderer->AddLine(from, to, pathColor);
				}
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
			else if (auto* mr = obj->GetComponent<MeshRenderer>(); mr && mr->HasModel()) {
				auto* modelData = mr->GetModel();
				BoundingBox localBounds(modelData->boundingBox.min, modelData->boundingBox.max);
				if (!localBounds.IsValid()) {
					localBounds = BoundingBox(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
				}

				const Transform& transform = obj->GetTransform();
				Matrix4x4 worldMatrix = transform.GetWorldMatrix();
				Matrix4x4 invWorldMatrix = worldMatrix.Inverse();

				Vector4 localOrigin4 = invWorldMatrix.TransformVector4(Vector4(rayOrigin.GetX(), rayOrigin.GetY(), rayOrigin.GetZ(), 1.0f));
				Vector4 localDir4 = invWorldMatrix.TransformVector4(Vector4(rayDir.GetX(), rayDir.GetY(), rayDir.GetZ(), 0.0f));

				Vector3 localRayOrigin(localOrigin4.GetX(), localOrigin4.GetY(), localOrigin4.GetZ());
				Vector3 localRayDir = Vector3(localDir4.GetX(), localDir4.GetY(), localDir4.GetZ()).Normalize();

				float tMin, tMax;
				if (localBounds.IntersectsRay(localRayOrigin, localRayDir, tMin, tMax)) {
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
					bool fHeld = ImGui::IsKeyDown(ImGuiKey_F);
					SelectObject(picked, fHeld);
					// オイラー角キャッシュをクリア（新しいオブジェクト選択時）
					cachedEulerAngles_.clear();
				}
			}
		}
	}

	void EditorUI::RenderBuildDialog() {
		if (!showBuildDialog_) return;

		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(U8("ゲームをエクスポート"), &showBuildDialog_)) {

			ImGui::TextWrapped(U8("ゲームをスタンドアロン実行ファイルとしてエクスポートします。"));
			ImGui::Separator();
			ImGui::Spacing();

			// 出力先パス
			ImGui::Text(U8("出力先フォルダ:"));
			static char outputPathBuf[512] = "";
			if (exportSettings_.outputPath.empty() && outputPathBuf[0] == '\0') {
				// デフォルトパスを設定
				std::filesystem::path defaultPath = std::filesystem::current_path() / "Build";
				wcstombs_s(nullptr, outputPathBuf, sizeof(outputPathBuf), defaultPath.wstring().c_str(), _TRUNCATE);
			} else if (!exportSettings_.outputPath.empty()) {
				wcstombs_s(nullptr, outputPathBuf, sizeof(outputPathBuf), exportSettings_.outputPath.c_str(), _TRUNCATE);
			}

			ImGui::SetNextItemWidth(-100);
			ImGui::InputText("##OutputPath", outputPathBuf, sizeof(outputPathBuf));
			ImGui::SameLine();
			if (ImGui::Button(U8("参照..."))) {
				std::wstring selectedPath = GameExporter::ShowFolderDialog(nullptr, L"出力先フォルダを選択");
				if (!selectedPath.empty()) {
					exportSettings_.outputPath = selectedPath;
					wcstombs_s(nullptr, outputPathBuf, sizeof(outputPathBuf), selectedPath.c_str(), _TRUNCATE);
				}
			}

			ImGui::Spacing();

			// ゲーム名
			ImGui::Text(U8("ゲーム名:"));
			static char gameNameBuf[128] = "Game";
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##GameName", gameNameBuf, sizeof(gameNameBuf));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// アセット設定
			ImGui::Text(U8("含めるアセット:"));
			ImGui::Checkbox(U8("シェーダー"), &exportSettings_.copyShaders);
			ImGui::Checkbox(U8("シーン"), &exportSettings_.copyScenes);
			ImGui::Checkbox(U8("モデル"), &exportSettings_.copyModels);
			ImGui::Checkbox(U8("テクスチャ"), &exportSettings_.copyTextures);
			ImGui::Checkbox(U8("オーディオ"), &exportSettings_.copyAudio);
		ImGui::Spacing();
		ImGui::Checkbox(U8("スマートコピー (シーン参照アセットのみ)"), &exportSettings_.smartAssetCopy);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(U8("ONにするとシーンJSONに参照されていないアセット(未使用音声・モデル等)を除外してサイズを削減します"));
		}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// async export completion check
			if (exportDone_.load()) {
				exportDone_.store(false);
				if (exportThread_.joinable()) exportThread_.join();
				buildInProgress_ = false;
				std::lock_guard<std::mutex> doneLock(buildMsgMutex_);
				if (exportSuccess_) {
					buildStatusMessage_ = U8("エクスポート完了!");
					consoleMessages_.push_back("[Build] Export successful: " + std::string(outputPathBuf));
				} else {
					buildStatusMessage_ = U8("エクスポート失敗: ") + exportError_;
					consoleMessages_.push_back("[Build] Export failed: " + exportError_);
				}
			}

			// progress bar while building
			if (buildInProgress_) {
				ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), ImVec2(-1, 0), "");
				ImGui::Spacing();
			}

			// status message (mutex protected)
			{
				std::lock_guard<std::mutex> msgLock(buildMsgMutex_);
				if (!buildStatusMessage_.empty()) {
				if (buildStatusMessage_.find("成功") != std::string::npos ||
					buildStatusMessage_.find("完了") != std::string::npos) {
					ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", buildStatusMessage_.c_str());
				} else if (buildStatusMessage_.find("失敗") != std::string::npos ||
						   buildStatusMessage_.find("エラー") != std::string::npos) {
					ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%s", buildStatusMessage_.c_str());
				} else {
					ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", buildStatusMessage_.c_str());
				}
				ImGui::Spacing();
			}
			}

			// ビルドログ（エラー時に表示）
			const auto& buildLog = gameExporter_.GetBuildLog();
			if (!buildLog.empty() && buildStatusMessage_.find("失敗") != std::string::npos) {
				ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
				if (ImGui::CollapsingHeader(U8("ビルドログ"))) {
					if (ImGui::Button(U8("ログをクリップボードにコピー"))) {
						ImGui::SetClipboardText(buildLog.c_str());
						buildLogCopied_ = true;
					}
					if (buildLogCopied_) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), U8("コピーしました!"));
					}
					ImGui::BeginChild("BuildLog", ImVec2(0, 150), ImGuiChildFlags_Borders);
					ImGui::TextWrapped("%s", buildLog.c_str());
					ImGui::EndChild();
				}
			} else {
				buildLogCopied_ = false;
			}

			// ビルドボタン
			ImGui::BeginDisabled(buildInProgress_);
			if (ImGui::Button(U8("エクスポート"), ImVec2(120, 0))) {
				// 設定を更新
				std::wstring wOutputPath(outputPathBuf, outputPathBuf + strlen(outputPathBuf));
				exportSettings_.outputPath = wOutputPath;

				std::wstring wGameName(gameNameBuf, gameNameBuf + strlen(gameNameBuf));
				exportSettings_.gameName = wGameName;

								buildInProgress_ = true;
				exportDone_.store(false);
				{
					std::lock_guard<std::mutex> lock(buildMsgMutex_);
					buildStatusMessage_ = U8("Shipping ビルド開始中...");
				}

				if (exportThread_.joinable()) exportThread_.join();
				ExportSettings settingsCopy = exportSettings_;
				exportThread_ = std::thread([this, settingsCopy]() {
					bool success = gameExporter_.Export(settingsCopy,
						[this](const ExportProgress& progress) {
							std::lock_guard<std::mutex> lock(buildMsgMutex_);
							buildStatusMessage_ = progress.currentTask;
						}
					);
					exportSuccess_ = success;
					if (!success) {
						exportError_ = gameExporter_.GetLastError();
					}
					exportDone_.store(true);
				});
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button(U8("閉じる"), ImVec2(120, 0))) {
				showBuildDialog_ = false;
			}

			ImGui::SameLine();
			if (ImGui::Button(U8("フォルダを開く"), ImVec2(120, 0))) {
				if (!exportSettings_.outputPath.empty()) {
					ShellExecuteW(nullptr, L"open", exportSettings_.outputPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				}
			}
		}
		ImGui::End();
	}

	void EditorUI::RenderObjectInspectorTab(const EditorContext& context) {
		// 選択されたオブジェクトの情報を表示
		GameObject* selected = selectedObject_ ? selectedObject_ : context.player;

		if (selected) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.75f, 1.0f));
			ImGui::TextUnformatted(selected->GetName().c_str());
			ImGui::PopStyleColor();
			ImGui::Separator();

			auto& transform = selected->GetTransform();
			auto pos   = transform.GetLocalPosition();
			auto rot   = transform.GetLocalRotation();
			auto scale = transform.GetLocalScale();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
			ImGui::TextUnformatted("Transform");
			ImGui::PopStyleColor();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.25f, 0.25f, 1.0f));
			ImGui::Text("  X"); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.75f, 0.25f, 1.0f));
			ImGui::Text("Y"); ImGui::SameLine();
			ImGui::PopStyleColor();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.40f, 0.90f, 1.0f));
			ImGui::Text("Z");
			ImGui::PopStyleColor();

			ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("位置  ")); ImGui::SameLine();
			ImGui::Text("(%.2f, %.2f, %.2f)", pos.GetX(), pos.GetY(), pos.GetZ());
			ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("回転  ")); ImGui::SameLine();
			ImGui::Text("(%.1f, %.1f, %.1f, %.1f)", rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());
			ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("スケール")); ImGui::SameLine();
			ImGui::Text("(%.2f, %.2f, %.2f)", scale.GetX(), scale.GetY(), scale.GetZ());

			// メッシュ情報の表示
			{
				auto* meshRenderer = selected->GetComponent<MeshRenderer>();
				auto* skinnedRenderer = selected->GetComponent<SkinnedMeshRenderer>();

				if (meshRenderer && meshRenderer->HasModel()) {
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.9f, 1.0f, 1.0f));
					ImGui::TextUnformatted("Mesh Info");
					ImGui::PopStyleColor();

					// モデルパス
					const auto& modelPath = meshRenderer->GetModelPath();
					if (!modelPath.empty()) {
						std::string fileName = modelPath.substr(modelPath.find_last_of("/\\") + 1);
						ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("モデル  ")); ImGui::SameLine();
						ImGui::TextUnformatted(fileName.c_str());
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("%s", modelPath.c_str());
						}
					}

					auto* model = meshRenderer->GetModel();
					const auto& meshes = model->meshes;
					uint32 totalVertices = 0;
					uint32 totalIndices = 0;

					for (const auto& mesh : meshes) {
						totalVertices += mesh.GetVertexBuffer().GetVertexCount();
						totalIndices += mesh.GetIndexBuffer().GetIndexCount();
					}

					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("メッシュ数")); ImGui::SameLine();
					ImGui::Text("%u", static_cast<uint32>(meshes.size()));
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("頂点数  ")); ImGui::SameLine();
					ImGui::Text("%u", totalVertices);
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("三角形数")); ImGui::SameLine();
					ImGui::Text("%u", totalIndices / 3);
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("インデックス数")); ImGui::SameLine();
					ImGui::Text("%u", totalIndices);

					// バウンディングボックス
					auto bbMin = model->boundingBox.min;
					auto bbMax = model->boundingBox.max;
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, "Bounds Min"); ImGui::SameLine();
					ImGui::Text("(%.2f, %.2f, %.2f)", bbMin.GetX(), bbMin.GetY(), bbMin.GetZ());
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, "Bounds Max"); ImGui::SameLine();
					ImGui::Text("(%.2f, %.2f, %.2f)", bbMax.GetX(), bbMax.GetY(), bbMax.GetZ());
					auto size = bbMax - bbMin;
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("サイズ  ")); ImGui::SameLine();
					ImGui::Text("(%.2f, %.2f, %.2f)", size.GetX(), size.GetY(), size.GetZ());

					// メッシュ詳細（折りたたみ）
					if (meshes.size() > 1 && ImGui::TreeNode(U8("メッシュ詳細"))) {
						for (size_t i = 0; i < meshes.size(); ++i) {
							const auto& mesh = meshes[i];
							std::string meshLabel = mesh.GetName().empty()
								? std::format("Mesh #{}", i)
								: mesh.GetName();

							if (ImGui::TreeNode(meshLabel.c_str())) {
								ImGui::Text(U8("  頂点: %u  三角形: %u"),
									mesh.GetVertexBuffer().GetVertexCount(),
									mesh.GetIndexBuffer().GetIndexCount() / 3);

								auto mMin = mesh.GetBoundsMin();
								auto mMax = mesh.GetBoundsMax();
								ImGui::Text("  Bounds: (%.2f,%.2f,%.2f) - (%.2f,%.2f,%.2f)",
									mMin.GetX(), mMin.GetY(), mMin.GetZ(),
									mMax.GetX(), mMax.GetY(), mMax.GetZ());

								if (mesh.HasMaterial()) {
									const auto& mat = mesh.GetMaterial()->GetData();
									if (!mat.name.empty()) {
										ImGui::Text(U8("  マテリアル: %s"), mat.name.c_str());
									}
									if (!mat.diffuseTexturePath.empty()) {
										std::string texName = mat.diffuseTexturePath.substr(
											mat.diffuseTexturePath.find_last_of("/\\") + 1);
										ImGui::Text(U8("  テクスチャ: %s"), texName.c_str());
									}
									ImGui::Text("  Metallic: %.2f  Roughness: %.2f", mat.metallic, mat.roughness);
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					} else if (meshes.size() == 1 && meshes[0].HasMaterial()) {
						const auto& mat = meshes[0].GetMaterial()->GetData();
						if (!mat.name.empty()) {
							ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("マテリアル")); ImGui::SameLine();
							ImGui::TextUnformatted(mat.name.c_str());
						}
						if (!mat.diffuseTexturePath.empty()) {
							std::string texName = mat.diffuseTexturePath.substr(
								mat.diffuseTexturePath.find_last_of("/\\") + 1);
							ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("テクスチャ")); ImGui::SameLine();
							ImGui::TextUnformatted(texName.c_str());
						}
						ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, "PBR"); ImGui::SameLine();
						ImGui::Text("Metallic: %.2f  Roughness: %.2f", mat.metallic, mat.roughness);
					}
				}
				else if (skinnedRenderer && skinnedRenderer->HasModel()) {
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.9f, 1.0f, 1.0f));
					ImGui::TextUnformatted("Skinned Mesh Info");
					ImGui::PopStyleColor();

					// モデルパス
					const auto& modelPath = skinnedRenderer->GetModelPath();
					if (!modelPath.empty()) {
						std::string fileName = modelPath.substr(modelPath.find_last_of("/\\") + 1);
						ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("モデル  ")); ImGui::SameLine();
						ImGui::TextUnformatted(fileName.c_str());
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("%s", modelPath.c_str());
						}
					}

					auto* modelData = skinnedRenderer->GetModelData();
					const auto& meshes = modelData->meshes;
					uint32 totalVertices = 0;
					uint32 totalIndices = 0;

					for (const auto& mesh : meshes) {
						totalVertices += mesh.GetVertexBuffer().GetVertexCount();
						totalIndices += mesh.GetIndexBuffer().GetIndexCount();
					}

					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("メッシュ数")); ImGui::SameLine();
					ImGui::Text("%u", static_cast<uint32>(meshes.size()));
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("頂点数  ")); ImGui::SameLine();
					ImGui::Text("%u", totalVertices);
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("三角形数")); ImGui::SameLine();
					ImGui::Text("%u", totalIndices / 3);
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("インデックス数")); ImGui::SameLine();
					ImGui::Text("%u", totalIndices);

					// ボーン情報
					if (modelData->skeleton) {
						ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("ボーン数")); ImGui::SameLine();
						ImGui::Text("%u", modelData->skeleton->GetBoneCount());
					}

					// アニメーション情報
					if (!modelData->animations.empty()) {
						ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, U8("アニメーション")); ImGui::SameLine();
						ImGui::Text("%u", static_cast<uint32>(modelData->animations.size()));
					}

					// バウンディングボックス
					auto bbMin = modelData->boundingBox.min;
					auto bbMax = modelData->boundingBox.max;
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, "Bounds Min"); ImGui::SameLine();
					ImGui::Text("(%.2f, %.2f, %.2f)", bbMin.GetX(), bbMin.GetY(), bbMin.GetZ());
					ImGui::TextColored({0.65f,0.65f,0.65f,1.f}, "Bounds Max"); ImGui::SameLine();
					ImGui::Text("(%.2f, %.2f, %.2f)", bbMax.GetX(), bbMax.GetY(), bbMax.GetZ());

					// メッシュ詳細（折りたたみ）
					if (meshes.size() > 1 && ImGui::TreeNode(U8("メッシュ詳細"))) {
						for (size_t i = 0; i < meshes.size(); ++i) {
							const auto& mesh = meshes[i];
							std::string meshLabel = mesh.GetName().empty()
								? std::format("Mesh #{}", i)
								: mesh.GetName();

							if (ImGui::TreeNode(meshLabel.c_str())) {
								ImGui::Text(U8("  頂点: %u  三角形: %u"),
									mesh.GetVertexBuffer().GetVertexCount(),
									mesh.GetIndexBuffer().GetIndexCount() / 3);

								if (mesh.HasMaterial()) {
									const auto& mat = mesh.GetMaterial()->GetData();
									if (!mat.name.empty()) {
										ImGui::Text(U8("  マテリアル: %s"), mat.name.c_str());
									}
									if (!mat.diffuseTexturePath.empty()) {
										std::string texName = mat.diffuseTexturePath.substr(
											mat.diffuseTexturePath.find_last_of("/\\") + 1);
										ImGui::Text(U8("  テクスチャ: %s"), texName.c_str());
									}
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
			}

			// NavAgentコンポーネントの表示
			auto* navAgent = selected->GetComponent<NavAgentComponent>();
			if (navAgent) {
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
				ImGui::Text("NavAgent");
				ImGui::PopStyleColor();

				float speed = navAgent->GetSpeed();
				if (ImGui::DragFloat(U8("移動速度"), &speed, 0.1f, 0.1f, 20.0f, "%.1f m/s")) {
					navAgent->SetSpeed(speed);
					isDirty_ = true;
				}

				float angularSpeed = navAgent->GetAngularSpeed();
				if (ImGui::DragFloat(U8("回転速度"), &angularSpeed, 1.0f, 1.0f, 720.0f, "%.0f deg/s")) {
					navAgent->SetAngularSpeed(angularSpeed);
					isDirty_ = true;
				}

				float acceleration = navAgent->GetAcceleration();
				if (ImGui::DragFloat(U8("加速度"), &acceleration, 0.1f, 0.1f, 50.0f, "%.1f m/s²")) {
					navAgent->SetAcceleration(acceleration);
					isDirty_ = true;
				}

				float stoppingDist = navAgent->GetStoppingDistance();
				if (ImGui::DragFloat(U8("停止距離"), &stoppingDist, 0.01f, 0.0f, 5.0f, "%.2f m")) {
					navAgent->SetStoppingDistance(stoppingDist);
					isDirty_ = true;
				}

				bool autoBrake = navAgent->IsAutobrake();
				if (ImGui::Checkbox(U8("自動減速"), &autoBrake)) {
					navAgent->SetAutobrake(autoBrake);
					isDirty_ = true;
				}

				// 状態表示
				ImGui::Spacing();
				const char* stateStr = "Idle";
				ImVec4 stateColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
				switch (navAgent->GetState()) {
					case NavAgentComponent::AgentState::Moving:
						stateStr = "Moving";
						stateColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
						break;
					case NavAgentComponent::AgentState::Arrived:
						stateStr = "Arrived";
						stateColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
						break;
					default:
						break;
				}
				ImGui::TextColored(stateColor, U8("状態: %s"), stateStr);

				if (navAgent->HasPath()) {
					ImGui::Text(U8("残り距離: %.2f m"), navAgent->GetRemainingDistance());
				}

				// パス可視化トグル
				bool visualize = navAgent->IsPathVisualized();
				if (ImGui::Checkbox(U8("パス表示"), &visualize)) {
					navAgent->SetPathVisualized(visualize);
				}

				// ライブ情報（リアルタイム監視）
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Text(U8("NavAgentライブ情報"));

				auto& navMeshManager = Navigation::NavMeshManager::Get();
				int agentIndex = navAgent->GetCrowdAgentIndex();
				bool crowdReady = navMeshManager.IsCrowdInitialized() && agentIndex >= 0;

				ImGui::Text(U8("Crowd: %s"), navMeshManager.IsCrowdInitialized() ? "ON" : "OFF");
				ImGui::Text(U8("Crowd Agent: %d"), agentIndex);
				ImGui::Text(U8("直進モード: %s"), navAgent->IsDirectMoveEnabled() ? "ON" : "OFF");

				DirectX::XMFLOAT3 agentPos;
				if (crowdReady) {
					agentPos = navMeshManager.GetAgentPosition(agentIndex);
				} else {
					auto worldPos = selected->GetTransform().GetPosition();
					agentPos = { worldPos.GetX(), worldPos.GetY(), worldPos.GetZ() };
				}

				auto velocity = navAgent->GetVelocity();
				float speedMagnitude = std::sqrt(
					velocity.x * velocity.x +
					velocity.y * velocity.y +
					velocity.z * velocity.z
				);

				ImGui::Text(U8("位置: (%.2f, %.2f, %.2f)"), agentPos.x, agentPos.y, agentPos.z);
				ImGui::Text(U8("速度: (%.2f, %.2f, %.2f)"), velocity.x, velocity.y, velocity.z);
				ImGui::Text(U8("速度m/s: %.2f"), speedMagnitude);

				bool isOnNavMesh = navMeshManager.IsPointOnNavMesh(agentPos);
				ImGui::Text(U8("NavMesh内: %s"), isOnNavMesh ? U8("はい") : U8("いいえ"));

				DirectX::XMFLOAT3 nextCorner = {0.0f, 0.0f, 0.0f};
				float distToCorner = -1.0f;
				bool hasNextCorner = false;
				if (crowdReady && navMeshManager.GetNextCorner(agentIndex, nextCorner, distToCorner)) {
					hasNextCorner = true;
					ImGui::Text(U8("次コーナー: (%.2f, %.2f, %.2f) [%.2f m]"),
						nextCorner.x, nextCorner.y, nextCorner.z, distToCorner);
				} else {
					ImGui::Text(U8("次コーナー: なし"));
				}

				const auto& dest = navAgent->GetDestination();
				ImGui::Text(U8("目的地: (%.2f, %.2f, %.2f)"), dest.x, dest.y, dest.z);
				ImGui::Text(U8("到達: %s"), navAgent->HasReachedDestination() ? U8("はい") : U8("いいえ"));
				ImGui::Text(U8("経路あり: %s"), navAgent->HasPath() ? U8("はい") : U8("いいえ"));

				ImGui::Spacing();
				bool logEnabled = navAgentLogEnabled_;
				if (ImGui::Checkbox(U8("Navログ出力"), &logEnabled)) {
					navAgentLogEnabled_ = logEnabled;
					navAgentLogTimer_ = 0.0f;
					navAgentLogHeaderWritten_ = false;
					navAgentLogErrorReported_ = false;
				}
				ImGui::SameLine();
				ImGui::TextDisabled("C:\\Users\\Unoryuto\\Documents\\navlog\\nav_agent_log.csv");

				if (navAgentLogEnabled_) {
					navAgentLogTimer_ += ImGui::GetIO().DeltaTime;
					while (navAgentLogTimer_ >= navAgentLogInterval_) {
						navAgentLogTimer_ -= navAgentLogInterval_;
						std::string line = std::format(
							"{:.3f},{},{},{},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{},{},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{}",
							ImGui::GetTime(),
							EscapeCsvValue(selected->GetName()),
							agentIndex,
							crowdReady ? 1 : 0,
							agentPos.x, agentPos.y, agentPos.z,
							velocity.x, velocity.y, velocity.z,
							speedMagnitude,
							isOnNavMesh ? 1 : 0,
							hasNextCorner ? 1 : 0,
							nextCorner.x, nextCorner.y, nextCorner.z,
							distToCorner,
							dest.x, dest.y, dest.z,
							EscapeCsvValue(stateStr)
						);
						AppendNavAgentLogLine(line);
					}
				}

				ImGui::Spacing();
				if (ImGui::Button(U8("コンポーネント削除"))) {
					selected->RemoveComponent<NavAgentComponent>();
				}
			}

			// EnemyDetectionComponentの表示
			auto* detection = selected->GetComponent<EnemyDetectionComponent>();
			if (detection) {
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.3f, 1.0f));
				ImGui::Text("Enemy Detection");
				ImGui::PopStyleColor();

				float detectionRange = detection->GetDetectionRange();
				if (ImGui::DragFloat(U8("検知距離"), &detectionRange, 0.1f, 1.0f, 50.0f, "%.1f m")) {
					detection->SetDetectionRange(detectionRange);
					isDirty_ = true;
				}

				float fov = detection->GetFieldOfView();
				if (ImGui::DragFloat(U8("視野角"), &fov, 1.0f, 10.0f, 360.0f, "%.0f deg")) {
					detection->SetFieldOfView(fov);
					isDirty_ = true;
				}

				float loseRange = detection->GetLoseRange();
				if (ImGui::DragFloat(U8("見失う距離"), &loseRange, 0.1f, 1.0f, 100.0f, "%.1f m")) {
					detection->SetLoseRange(loseRange);
					isDirty_ = true;
				}

				float lostWaitTime = detection->GetLostWaitTime();
				if (ImGui::DragFloat(U8("見失い後の待機"), &lostWaitTime, 0.1f, 0.0f, 30.0f, "%.1f s")) {
					detection->SetLostWaitTime(lostWaitTime);
					isDirty_ = true;
				}

				float wanderRadius = detection->GetWanderRadius();
				if (ImGui::DragFloat(U8("徘徊範囲"), &wanderRadius, 0.5f, 1.0f, 100.0f, "%.1f m")) {
					detection->SetWanderRadius(wanderRadius);
					isDirty_ = true;
				}

				// ターゲット選択（ドロップダウン）
				std::string currentTarget = detection->GetTargetName();
				std::string displayName = currentTarget.empty() ? "(None)" : currentTarget;

				if (ImGui::BeginCombo(U8("ターゲット"), displayName.c_str())) {
					// (None) 選択肢
					if (ImGui::Selectable("(None)", currentTarget.empty())) {
						detection->SetTargetName("");
						isDirty_ = true;
					}

					// シーン内の全GameObjectをリスト
					for (const auto& obj : *gameObjects_) {
						if (obj.get() == selected) continue; // 自分自身は除外

						const std::string& objName = obj->GetName();
						bool isSelected = (objName == currentTarget);

						if (ImGui::Selectable(objName.c_str(), isSelected)) {
							detection->SetTargetName(objName);
							isDirty_ = true;
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				// 状態表示
				ImGui::Spacing();
				const char* stateStr = "Idle";
				ImVec4 stateColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
				switch (detection->GetState()) {
					case EnemyDetectionComponent::State::Chasing:
						stateStr = "Chasing";
						stateColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
						break;
					case EnemyDetectionComponent::State::LostTarget:
						stateStr = "Lost Target";
						stateColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
						break;
					default:
						break;
				}
				ImGui::TextColored(stateColor, U8("状態: %s"), stateStr);

				ImGui::Spacing();
				if (ImGui::Button(U8("Enemy Detection削除"))) {
					selected->RemoveComponent<EnemyDetectionComponent>();
					isDirty_ = true;
				}
			}

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

				std::string displayName = currentScript.empty() ? "(None)" :
					currentScript.substr(currentScript.find_last_of("/\\") + 1);

				if (ImGui::BeginCombo("Script", displayName.c_str())) {
					if (ImGui::Selectable("(None)", currentScript.empty())) {
						luaScript->SetScriptPath("");
						isDirty_ = true;
					}

					for (size_t i = 0; i < cachedScriptPaths_.size(); ++i) {
						std::string scriptName = cachedScriptPaths_[i].substr(
							cachedScriptPaths_[i].find_last_of("/\\") + 1);
						bool isSelected = (currentIndex == static_cast<int>(i));

						if (ImGui::Selectable(scriptName.c_str(), isSelected)) {
							luaScript->SetScriptPath(cachedScriptPaths_[i]);
							(void)luaScript->ReloadScript();
							isDirty_ = true;
						}

						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("%s", cachedScriptPaths_[i].c_str());
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				if (ImGui::Button("R##RefreshScripts")) {
					RefreshScriptPaths();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Refresh script list");
				}

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

				ImGui::Spacing();
				if (ImGui::Button("Reload Script")) {
					(void)luaScript->ReloadScript();
							isDirty_ = true;
				}

				ImGui::SameLine();
				if (ImGui::Button("Remove Script")) {
					selected->RemoveComponent<LuaScriptComponent>();
				}
			}

			// コンポーネント追加セクション
			ImGui::Separator();
			ImGui::Text(U8("コンポーネント追加"));
			
			if (!luaScript) {
				if (ImGui::Button("Add Lua Script")) {
					selected->AddComponent<LuaScriptComponent>();
					RefreshScriptPaths();
				}
			}
			
			if (!navAgent) {
				ImGui::SameLine();
				if (ImGui::Button("Add NavAgent")) {
					selected->AddComponent<NavAgentComponent>();
					isDirty_ = true;
				}
			}

			if (!detection) {
				ImGui::SameLine();
				if (ImGui::Button("Add Enemy Detection")) {
					selected->AddComponent<EnemyDetectionComponent>();
					isDirty_ = true;
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

		float rotateSpeed = editorCamera_.GetRotateSpeed();
		ImGui::Text("Mouse Sensitivity");
		if (ImGui::SliderFloat("##MouseSensitivity", &rotateSpeed, 0.1f, 5.0f, "%.2f")) {
			editorCamera_.SetRotateSpeed(rotateSpeed);
			settingsChanged = true;
		}

		float moveSpeed = editorCamera_.GetMoveSpeed();
		ImGui::Text("Move Speed");
		if (ImGui::SliderFloat("##MoveSpeed", &moveSpeed, 1.0f, 100.0f, "%.1f")) {
			editorCamera_.SetMoveSpeed(moveSpeed);
			settingsChanged = true;
		}

		float scrollSpeed = editorCamera_.GetScrollSpeed();
		ImGui::Text("Scroll Speed");
		if (ImGui::SliderFloat("##ScrollSpeed", &scrollSpeed, 0.1f, 5.0f, "%.2f")) {
			editorCamera_.SetScrollSpeed(scrollSpeed);
			settingsChanged = true;
		}

		if (settingsChanged) {
			editorCamera_.SaveSettings();
		}
	}

	void EditorUI::RenderNavMeshInspectorTab() {
		auto& navMesh = Navigation::NavMeshManager::Get();
		auto settings = navMesh.GetSettings();
		bool settingsChanged = false;
		bool isBaking = navMeshBaking_.load();

		// ステータス表示
		if (isBaking) {
			ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), U8("◎ ベイク中..."));
		} else if (navMesh.IsBuilt()) {
			auto stats = navMesh.GetStats();
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), U8("● ビルド済み"));
			ImGui::Text(U8("  ポリゴン: %d / 頂点: %d"), stats.polyCount, stats.vertexCount);
			ImGui::Text(U8("  タイル: %d / %.2f KB"), stats.tileCount, stats.memoryUsage / 1024.0f);
			ImGui::Text(U8("  ビルド時間: %.2f秒"), stats.buildTimeSeconds);
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), U8("○ 未ビルド"));
		}

		ImGui::Separator();

		// 表示設定
		if (ImGui::Checkbox(U8("NavMesh表示"), &showRecastNavMesh_)) {
			navMesh.SetDebugDrawEnabled(showRecastNavMesh_);
		}

		auto color = navMesh.GetDebugDrawColor();
		float colorArr[4] = {color.x, color.y, color.z, color.w};
		if (ImGui::ColorEdit4(U8("表示色"), colorArr)) {
			navMesh.SetDebugDrawColor({colorArr[0], colorArr[1], colorArr[2], colorArr[3]});
		}

		ImGui::Separator();

		// ビルド設定
		if (ImGui::CollapsingHeader(U8("ボクセル設定"), ImGuiTreeNodeFlags_DefaultOpen)) {
			settingsChanged |= ImGui::DragFloat(U8("セルサイズ"), &settings.cellSize, 0.01f, 0.05f, 1.0f, "%.2f m");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("小さいほど精度が上がるが処理が重くなる"));
			settingsChanged |= ImGui::DragFloat(U8("セル高さ"), &settings.cellHeight, 0.01f, 0.05f, 1.0f, "%.2f m");
		}

		if (ImGui::CollapsingHeader(U8("エージェント設定"), ImGuiTreeNodeFlags_DefaultOpen)) {
			settingsChanged |= ImGui::DragFloat(U8("半径"), &settings.agentRadius, 0.01f, 0.1f, 5.0f, "%.2f m");
			settingsChanged |= ImGui::DragFloat(U8("高さ"), &settings.agentHeight, 0.1f, 0.5f, 10.0f, "%.1f m");
			settingsChanged |= ImGui::DragFloat(U8("段差許容"), &settings.agentMaxClimb, 0.01f, 0.0f, 2.0f, "%.2f m");
			settingsChanged |= ImGui::DragFloat(U8("最大傾斜角"), &settings.agentMaxSlope, 1.0f, 0.0f, 90.0f, "%.0f deg");
		}

		if (ImGui::CollapsingHeader(U8("メッシュ生成"))) {
			settingsChanged |= ImGui::DragFloat(U8("単純化誤差"), &settings.maxSimplificationError, 0.1f, 0.0f, 5.0f, "%.1f");
			settingsChanged |= ImGui::DragFloat(U8("詳細サンプル距離"), &settings.detailSampleDist, 0.5f, 0.0f, 20.0f, "%.1f");
			settingsChanged |= ImGui::DragFloat(U8("詳細サンプル誤差"), &settings.detailSampleMaxError, 0.1f, 0.0f, 5.0f, "%.1f");
		}

		if (ImGui::CollapsingHeader(U8("タイリング"))) {
			settingsChanged |= ImGui::DragInt(U8("最大タイル数"), &settings.maxTiles, 1, 1, 256);
			settingsChanged |= ImGui::DragInt(U8("タイルサイズ"), &settings.tileSize, 1, 16, 128);
		}

		if (ImGui::CollapsingHeader(U8("フィルタリング"))) {
			settingsChanged |= ImGui::Checkbox(U8("Monotone分割"), &settings.useMonotone);
			settingsChanged |= ImGui::Checkbox(U8("低い障害物を除外"), &settings.filterLowHangingObstacles);
			settingsChanged |= ImGui::Checkbox(U8("崖スパンを除外"), &settings.filterLedgeSpans);
			settingsChanged |= ImGui::Checkbox(U8("低い天井を除外"), &settings.filterWalkableLowHeightSpans);
		}

		if (settingsChanged) {
			navMesh.SetSettings(settings);
		}

		ImGui::Separator();

		// アクションボタン
		ImGui::BeginDisabled(isBaking);
		if (ImGui::Button(isBaking ? U8("ベイク中...") : U8("ベイク"), ImVec2(-1, 0))) {
			BakeNavMesh();
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(isBaking);
		if (navMesh.IsBuilt()) {
			if (ImGui::Button(U8("クリア"), ImVec2(-1, 0))) {
				navMesh.Shutdown();
				navMesh.Initialize();
				showRecastNavMesh_ = false;
				AddConsoleMessage(U8("[NavMesh] クリアしました"));
			}

			ImGui::Spacing();

			if (ImGui::Button(U8("保存..."), ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 4, 0))) {
				OPENFILENAMEA ofn = {};
				char filename[MAX_PATH] = "";
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = nullptr;
				ofn.lpstrFilter = "NavMesh (*.navmesh)\0*.navmesh\0All Files (*.*)\0*.*\0";
				ofn.lpstrFile = filename;
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "navmesh";
				ofn.Flags = OFN_OVERWRITEPROMPT;

				if (GetSaveFileNameA(&ofn)) {
					if (navMesh.SaveNavMesh(filename)) {
						AddConsoleMessage(U8("[NavMesh] 保存: ") + std::string(filename));
					} else {
						AddConsoleMessage(U8("[NavMesh] 保存失敗"));
					}
				}
			}

			ImGui::SameLine();

			if (ImGui::Button(U8("読み込み..."), ImVec2(-1, 0))) {
				OPENFILENAMEA ofn = {};
				char filename[MAX_PATH] = "";
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = nullptr;
				ofn.lpstrFilter = "NavMesh (*.navmesh)\0*.navmesh\0All Files (*.*)\0*.*\0";
				ofn.lpstrFile = filename;
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_FILEMUSTEXIST;

				if (GetOpenFileNameA(&ofn)) {
					if (navMesh.LoadNavMesh(filename)) {
						AddConsoleMessage(U8("[NavMesh] 読み込み: ") + std::string(filename));
						showRecastNavMesh_ = true;
						navMesh.SetDebugDrawEnabled(true);
					} else {
						AddConsoleMessage(U8("[NavMesh] 読み込み失敗"));
					}
				}
			}
		}
		ImGui::EndDisabled();

		// NavAgent追加セクション
		ImGui::Separator();
		if (ImGui::CollapsingHeader(U8("NavAgent"), ImGuiTreeNodeFlags_DefaultOpen)) {
			if (selectedObject_) {
				auto* navAgent = selectedObject_->GetComponent<NavAgentComponent>();
				
				ImGui::Text(U8("選択: %s"), selectedObject_->GetName().c_str());
				
				if (navAgent) {
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), U8("● NavAgent あり"));
					
					// NavAgent設定
					float speed = navAgent->GetSpeed();
					if (ImGui::DragFloat(U8("移動速度"), &speed, 0.1f, 0.1f, 20.0f, "%.1f m/s")) {
						navAgent->SetSpeed(speed);
					}
					
					float angularSpeed = navAgent->GetAngularSpeed();
					if (ImGui::DragFloat(U8("回転速度"), &angularSpeed, 1.0f, 10.0f, 720.0f, "%.0f deg/s")) {
						navAgent->SetAngularSpeed(angularSpeed);
					}
					
					float stoppingDist = navAgent->GetStoppingDistance();
					if (ImGui::DragFloat(U8("停止距離"), &stoppingDist, 0.01f, 0.0f, 5.0f, "%.2f m")) {
						navAgent->SetStoppingDistance(stoppingDist);
					}
					
					ImGui::Spacing();
					if (ImGui::Button(U8("NavAgentを削除"), ImVec2(-1, 0))) {
						selectedObject_->RemoveComponent<NavAgentComponent>();
						isDirty_ = true;
					}
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), U8("○ NavAgent なし"));
					
					if (ImGui::Button(U8("NavAgentを追加"), ImVec2(-1, 0))) {
						selectedObject_->AddComponent<NavAgentComponent>();
						isDirty_ = true;
					}
				}
			} else {
				ImGui::TextDisabled(U8("オブジェクトを選択してください"));
			}
		}
	}

	void EditorUI::AppendNavAgentLogLine(const std::string& line) {
		if (navAgentLogErrorReported_) {
			return;
		}

		std::error_code ec;
		std::filesystem::create_directories(kNavLogDir, ec);
		if (ec) {
			AddConsoleMessage(std::format("[NavLog] Failed to create directory: {}", kNavLogDir.string()));
			navAgentLogErrorReported_ = true;
			return;
		}

		bool needsHeader = !navAgentLogHeaderWritten_;
		if (needsHeader) {
			std::error_code sizeEc;
			if (std::filesystem::exists(kNavLogFile, sizeEc)) {
				auto size = std::filesystem::file_size(kNavLogFile, sizeEc);
				if (!sizeEc && size > 0) {
					needsHeader = false;
					navAgentLogHeaderWritten_ = true;
				}
			}
		}

		std::ofstream file(kNavLogFile, std::ios::app);
		if (!file.is_open()) {
			AddConsoleMessage(std::format("[NavLog] Failed to open log file: {}", kNavLogFile.string()));
			navAgentLogErrorReported_ = true;
			return;
		}

		if (needsHeader) {
			file << "time,object,agentIndex,crowdReady,posX,posY,posZ,velX,velY,velZ,speed,onNavMesh,hasNextCorner,"
					"nextCornerX,nextCornerY,nextCornerZ,distToCorner,destX,destY,destZ,state\n";
			navAgentLogHeaderWritten_ = true;
		}

		file << line << '\n';
	}

	void EditorUI::BakeNavMesh() {
		if (!scene_) {
			AddConsoleMessage(U8("[NavMesh] シーンがありません"));
			return;
		}

		if (navMeshBaking_.load()) {
			AddConsoleMessage(U8("[NavMesh] 既にベイク中です"));
			return;
		}

		// シーンからジオメトリを収集（メインスレッドで実行）
		std::vector<Navigation::StaticGeometry> geometryList;

		for (const auto& obj : scene_->GetGameObjects()) {
			auto* meshRenderer = obj->GetComponent<MeshRenderer>();
			if (!meshRenderer) continue;

			const auto& transform = obj->GetTransform();
			DirectX::XMFLOAT3 position(
				transform.GetPosition().GetX(),
				transform.GetPosition().GetY(),
				transform.GetPosition().GetZ()
			);

			if (meshRenderer->HasModel()) {
				const auto& meshes = meshRenderer->GetMeshes();
				for (const auto& mesh : meshes) {
					if (!mesh.HasCPUData()) continue;

					Navigation::StaticGeometry geom;
					geom.position = position;

					const auto& vertices = mesh.GetVertices();
					const auto& indices = mesh.GetIndices();

					geom.vertices.reserve(vertices.size());
					for (const auto& v : vertices) {
						geom.vertices.emplace_back(v.px, v.py, v.pz);
					}

					geom.indices.reserve(indices.size());
					for (const auto& idx : indices) {
						geom.indices.push_back(idx);
					}

					geometryList.push_back(std::move(geom));
				}
			} else if (auto* singleMesh = meshRenderer->GetMesh()) {
				if (!singleMesh->HasCPUData()) continue;

				Navigation::StaticGeometry geom;
				geom.position = position;

				const auto& vertices = singleMesh->GetVertices();
				const auto& indices = singleMesh->GetIndices();

				geom.vertices.reserve(vertices.size());
				for (const auto& v : vertices) {
					geom.vertices.emplace_back(v.px, v.py, v.pz);
				}

				geom.indices.reserve(indices.size());
				for (const auto& idx : indices) {
					geom.indices.push_back(idx);
				}

				geometryList.push_back(std::move(geom));
			}
		}

		if (geometryList.empty()) {
			AddConsoleMessage("[NavMesh] No valid meshes in scene");
			return;
		}

		AddConsoleMessage("[NavMesh] Collected " + std::to_string(geometryList.size()) + " meshes");

		// 非同期ベイク開始
		navMeshBaking_.store(true);
		navMeshBakeProgress_.store(0.0f);
		{
			std::lock_guard<std::mutex> lock(navMeshBakeMutex_);
			navMeshBakeStage_ = U8("初期化中...");
		}

		// UIで編集した設定を使用
		auto& navMeshManager = Navigation::NavMeshManager::Get();
		auto settings = navMeshManager.GetSettings();
		
		// ビルド開始フラグを設定（Crowd更新を防ぐ）
		navMeshManager.SetBuilding(true);

		navMeshBakeFuture_ = std::async(std::launch::async, [this, geometryList = std::move(geometryList), settings]() {
			auto& navMeshManager = Navigation::NavMeshManager::Get();
			navMeshManager.Initialize();

			navMeshManager.SetProgressCallback([this](float progress, const char* stage) {
				navMeshBakeProgress_.store(progress);
				std::lock_guard<std::mutex> lock(navMeshBakeMutex_);
				navMeshBakeStage_ = stage;
			});

			bool result = navMeshManager.BuildNavMesh(geometryList, settings);
			
			// ビルド完了フラグをリセット
			navMeshManager.SetBuilding(false);
			navMeshBaking_.store(false);
			return result;
		});
	}

	// ============================================================
	// 草ペイントツール
	// ============================================================

	void EditorUI::RenderGrassPaintTab() {
		if (!renderer_) return;
		auto* grassSystem = renderer_->GetGrassSystem();
		auto* grassRenderer = renderer_->GetGrassRenderer();
		if (!grassSystem || !grassRenderer) return;

		ImGui::Spacing();

		// === テクスチャ選択 ===
		ImGui::Text(U8("草テクスチャ"));
		{
			const auto& currentPath = grassRenderer->GetTexturePath();
			if (currentPath.empty()) {
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), U8("(未設定 - 白テクスチャ)"));
			} else {
				// ファイル名のみ表示
				namespace fs = std::filesystem;
				std::string filename = fs::path(currentPath).filename().string();
				ImGui::TextWrapped("%s", filename.c_str());
			}

			if (ImGui::Button(U8("テクスチャを選択..."), ImVec2(-1, 0))) {
				grassTextureBrowseOpen_ = true;
				grassTextureScanned_ = false;
			}
		}

		// テクスチャ選択ポップアップ
		if (grassTextureBrowseOpen_) {
			ImGui::OpenPopup(U8("草テクスチャ選択"));
			grassTextureBrowseOpen_ = false;
		}

		if (ImGui::BeginPopupModal(U8("草テクスチャ選択"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			// 初回スキャン
			if (!grassTextureScanned_) {
				grassTextureCandidates_.clear();
				namespace fs = std::filesystem;
				if (fs::exists("assets")) {
					for (auto& entry : fs::recursive_directory_iterator("assets")) {
						if (!entry.is_regular_file()) continue;
						auto ext = entry.path().extension().string();
						// 小文字に変換
						for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
							grassTextureCandidates_.push_back(entry.path().string());
						}
					}
				}
				// パス区切りを統一
				for (auto& p : grassTextureCandidates_) {
					std::replace(p.begin(), p.end(), '\\', '/');
				}
				grassTextureScanned_ = true;
			}

			ImGui::Text(U8("assets/ 内の画像ファイル (%d 件)"), static_cast<int>(grassTextureCandidates_.size()));
			ImGui::Separator();

			ImGui::BeginChild("TextureList", ImVec2(450, 300), true);
			for (size_t i = 0; i < grassTextureCandidates_.size(); ++i) {
				namespace fs = std::filesystem;
				std::string display = fs::path(grassTextureCandidates_[i]).filename().string();
				std::string relDir = fs::path(grassTextureCandidates_[i]).parent_path().string();

				bool selected = (grassTextureCandidates_[i] == grassRenderer->GetTexturePath());
				if (ImGui::Selectable((display + "##" + std::to_string(i)).c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
					// テクスチャ読み込み
					if (graphics_) {
						graphics_->BeginResourceUpload();
						if (grassRenderer->LoadTextureFromFile(grassTextureCandidates_[i])) {
							grassSystem->SetGrassTexturePath(grassTextureCandidates_[i]);
							consoleMessages_.push_back(
								U8("[草原] テクスチャ変更: ") + display);
						}
						graphics_->EndResourceUpload();
					}
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s", grassTextureCandidates_[i].c_str());
				}
			}
			ImGui::EndChild();

			if (ImGui::Button(U8("キャンセル"), ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// === モード切り替え ===
		ImGui::Text(U8("ペイントモード"));
		ImGui::SameLine();
		bool isBrush = (grassPaintMode_ == GrassPaintMode::Brush);
		bool isStamp = (grassPaintMode_ == GrassPaintMode::Stamp);
		bool isErase = (grassPaintMode_ == GrassPaintMode::Erase);

		if (ImGui::RadioButton(U8("ブラシ"), isBrush)) grassPaintMode_ = GrassPaintMode::Brush;
		ImGui::SameLine();
		if (ImGui::RadioButton(U8("スタンプ"), isStamp)) grassPaintMode_ = GrassPaintMode::Stamp;
		ImGui::SameLine();
		if (ImGui::RadioButton(U8("消去"), isErase)) grassPaintMode_ = GrassPaintMode::Erase;

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// ペイントON/OFF
		if (grassPaintActive_) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
			if (ImGui::Button(U8("ペイント ON (Scene Viewでクリック)"), ImVec2(-1, 30))) {
				grassPaintActive_ = false;
			}
			ImGui::PopStyleColor();
		} else {
			if (ImGui::Button(U8("ペイント OFF (クリックで有効化)"), ImVec2(-1, 30))) {
				grassPaintActive_ = true;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// パラメータ
		ImGui::Text(U8("ブラシ設定"));
		ImGui::SliderFloat(U8("半径"), &grassBrushRadius_, 0.5f, 20.0f, "%.1f m");
		ImGui::SliderFloat(U8("密度"), &grassDensity_, 1.0f, 50.0f, "%.0f /m2");

		ImGui::Spacing();
		ImGui::Text(U8("草パラメータ"));
		ImGui::SliderFloat(U8("最小スケール"), &grassMinScale_, 0.1f, 2.0f, "%.2f");
		ImGui::SliderFloat(U8("最大スケール"), &grassMaxScale_, 0.1f, 3.0f, "%.2f");
		ImGui::SliderFloat(U8("色変化"), &grassColorVariation_, 0.0f, 1.0f, "%.2f");

		ImGui::Spacing();
		ImGui::Text(U8("草サイズ"));
		float bw = grassRenderer->GetBaseWidth();
		float bh = grassRenderer->GetBaseHeight();
		if (ImGui::SliderFloat(U8("幅"), &bw, 0.1f, 5.0f, "%.2f")) {
			grassRenderer->SetBaseWidth(bw);
		}
		if (ImGui::SliderFloat(U8("高さ"), &bh, 0.1f, 5.0f, "%.2f")) {
			grassRenderer->SetBaseHeight(bh);
		}

		ImGui::Spacing();
		ImGui::Text(U8("風"));
		ImGui::SliderFloat(U8("風の強さ"), &grassWindStrength_, 0.0f, 1.0f, "%.2f");
		grassRenderer->SetWindStrength(grassWindStrength_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 統計
		ImGui::Text(U8("草の本数: %d"), grassSystem->GetInstanceCount());

		if (ImGui::Button(U8("全てクリア"), ImVec2(-1, 0))) {
			grassSystem->Clear();
		}

		ImGui::Spacing();

		// テスト配置ボタン
		if (ImGui::Button(U8("テスト: 原点に100本配置"), ImVec2(-1, 0))) {
			grassSystem->AddInstancesInRadius(0.0f, 0.0f, 0.0f, 5.0f,
				grassDensity_, grassMinScale_, grassMaxScale_, grassColorVariation_);
			grassSystem->SetDirty();
		}
	}

	Vector3 EditorUI::ScreenToGroundPosition(float screenX, float screenY) {
		// Scene Viewのカメラからレイを飛ばしてY=0平面と交差
		auto& cam = sceneViewCamera_;
		auto viewMatrix = cam.GetViewMatrix();
		auto projMatrix = cam.GetProjectionMatrix();

		// NDC座標に変換
		float ndcX = (2.0f * (screenX - sceneViewPosX_) / sceneViewSizeX_) - 1.0f;
		float ndcY = 1.0f - (2.0f * (screenY - sceneViewPosY_) / sceneViewSizeY_);

		// 逆投影行列でワールド空間のレイを構築
		auto invProj = projMatrix.Inverse();
		auto invView = viewMatrix.Inverse();

		// Near plane上の点
		Vector3 nearNDC(ndcX, ndcY, 0.0f);
		Vector3 farNDC(ndcX, ndcY, 1.0f);

		// NDC → View space
		auto unprojectPoint = [&](const Vector3& ndc) -> Vector3 {
			float x = ndc.GetX();
			float y = ndc.GetY();
			float z = ndc.GetZ();

			// invProj を使って view space に変換
			// 4x4行列の各要素にアクセスする簡易計算
			// w = invProj[3][2] * z + invProj[3][3]
			float viewArr[16];
			invProj.ToFloatArray(viewArr);

			float vx = viewArr[0] * x + viewArr[4] * y + viewArr[8] * z + viewArr[12];
			float vy = viewArr[1] * x + viewArr[5] * y + viewArr[9] * z + viewArr[13];
			float vz = viewArr[2] * x + viewArr[6] * y + viewArr[10] * z + viewArr[14];
			float vw = viewArr[3] * x + viewArr[7] * y + viewArr[11] * z + viewArr[15];

			if (std::abs(vw) > 0.0001f) {
				vx /= vw; vy /= vw; vz /= vw;
			}

			// View space → World space
			float invViewArr[16];
			invView.ToFloatArray(invViewArr);

			float wx = invViewArr[0] * vx + invViewArr[4] * vy + invViewArr[8] * vz + invViewArr[12];
			float wy = invViewArr[1] * vx + invViewArr[5] * vy + invViewArr[9] * vz + invViewArr[13];
			float wz = invViewArr[2] * vx + invViewArr[6] * vy + invViewArr[10] * vz + invViewArr[14];

			return Vector3(wx, wy, wz);
		};

		Vector3 nearWorld = unprojectPoint(nearNDC);
		Vector3 farWorld = unprojectPoint(farNDC);

		// レイ方向
		Vector3 rayDir = farWorld - nearWorld;
		float len = rayDir.Length();
		if (len > 0.0001f) rayDir = rayDir * (1.0f / len);

		Vector3 rayOrigin = nearWorld;

		// Y=0平面との交差
		float denom = rayDir.GetY();
		if (std::abs(denom) < 0.0001f) {
			return Vector3(0, 0, 0); // 平行
		}
		float t = -rayOrigin.GetY() / denom;
		if (t < 0) {
			return Vector3(0, 0, 0); // カメラの後ろ
		}

		return rayOrigin + rayDir * t;
	}

	void EditorUI::HandleGrassPainting() {
		if (!grassPaintActive_ || !renderer_) return;
		if (editorMode_ != EditorMode::Edit) return;

		auto* grassSystem = renderer_->GetGrassSystem();
		if (!grassSystem) return;

		auto& io = ImGui::GetIO();

		// Scene View内でマウスが押されている場合
		float mouseX = io.MousePos.x;
		float mouseY = io.MousePos.y;

		// Scene Viewの範囲内かチェック
		if (mouseX < sceneViewPosX_ || mouseX > sceneViewPosX_ + sceneViewSizeX_ ||
			mouseY < sceneViewPosY_ || mouseY > sceneViewPosY_ + sceneViewSizeY_) {
			return;
		}

		bool isLeftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
		bool isLeftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

		if (!isLeftDown && !isLeftClicked) return;

		Vector3 groundPos = ScreenToGroundPosition(mouseX, mouseY);

		if (grassPaintMode_ == GrassPaintMode::Erase) {
			if (isLeftDown) {
				grassSystem->RemoveInstancesInRadius(
					groundPos.GetX(), groundPos.GetY(), groundPos.GetZ(),
					grassBrushRadius_);
			}
		} else if (grassPaintMode_ == GrassPaintMode::Stamp) {
			if (isLeftClicked) {
				grassSystem->AddInstancesInRadius(
					groundPos.GetX(), groundPos.GetY(), groundPos.GetZ(),
					grassBrushRadius_, grassDensity_,
					grassMinScale_, grassMaxScale_, grassColorVariation_);
				grassSystem->SetDirty();
			}
		} else { // Brush
			if (isLeftDown) {
				grassPaintCooldown_ -= io.DeltaTime;
				if (grassPaintCooldown_ <= 0.0f) {
					// ブラシ密度をフレーム間隔に合わせて調整
					float effectiveDensity = grassDensity_ * 0.3f; // ブラシは軽めに
					grassSystem->AddInstancesInRadius(
						groundPos.GetX(), groundPos.GetY(), groundPos.GetZ(),
						grassBrushRadius_, effectiveDensity,
						grassMinScale_, grassMaxScale_, grassColorVariation_);
					grassSystem->SetDirty();
					grassPaintCooldown_ = 0.05f; // 50msインターバル
				}
			} else {
				grassPaintCooldown_ = 0.0f;
			}
		}
	}

	// ============================================================
	// シネマティックエディタ更新（GameApplicationから毎フレーム呼ぶ）
	// ============================================================
	void EditorUI::UpdateCinematicEditor(float deltaTime) {
		cinematicEditor_.Update(deltaTime);
	}

} // namespace UnoEngine
