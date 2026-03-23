#include "pch.h"
#include "GameApplication.h"
#include "../Engine/Core/Scene.h"
#include "../Engine/Resource/ResourceLoader.h"
#include "../Engine/Rendering/RenderSystem.h"
#include "../Engine/Rendering/SkinnedRenderItem.h"
#include "../Engine/Audio/AudioSystem.h"
#include "../Engine/Systems/CollisionSystem.h"
#include "../Engine/Systems/PhysicsSystem.h"
#include "../Engine/Systems/MeshCollisionSystem.h"
#include "../Engine/Core/Logger.h"
#include "../Engine/Video/VideoPlayerComponent.h"
#include "../Engine/Rendering/LightManager.h"
#include "../Engine/Scene/SceneSerializer.h"
#include "../Engine/Cinematic/CinematicManager.h"
#include "../Engine/Cinematic/CinematicSequence.h"
#ifdef WITH_EDITOR
#include "../Engine/Graphics/MeshRenderer.h"
#include "../Engine/Rendering/SkinnedMeshRenderer.h"
#include <thread>
#include <atomic>
#include <chrono>
#endif

namespace UnoEngine {

void GameApplication::OnInit() {
    // Initialize ResourceManager
    resourceManager_ = std::make_unique<ResourceManager>(graphics_.get());
    Logger::Info("[初期化] ResourceManager 準備完了");

    // Register systems
    GetSystemManager()->RegisterSystem<AnimationSystem>();
    GetSystemManager()->RegisterSystem<CameraSystem>();
    GetSystemManager()->RegisterSystem<AudioSystem>();
    GetSystemManager()->RegisterSystem<CollisionSystem>();
    GetSystemManager()->RegisterSystem<PhysicsSystem>();
    GetSystemManager()->RegisterSystem<MeshCollisionSystem>();
    Logger::Info("[初期化] システム登録完了 (Animation, Camera, Audio, Collision, Physics, MeshCollision)");
}

void GameApplication::OnUpdate(float deltaTime) {
    // シネマティックの遅延ロード（シーンロード後にカメラが確定してから）
    if (!cinematicsLoaded_) {
        Scene* scene = GetSceneManager()->GetActiveScene();
        Camera* cam = scene ? scene->GetActiveCamera() : nullptr;
        if (cam) {
            cinematicManager_.SetCamera(cam);
            for (const auto& [name, path] : SceneSerializer::s_cinematicPaths) {
                cinematicManager_.RegisterFromFile(name, path);
            }
#ifndef WITH_EDITOR
            // リリースビルドではintroを自動再生
            if (SceneSerializer::s_cinematicPaths.contains("intro")) {
                cinematicManager_.Play("intro");
                Logger::Info("[CinematicManager] Intro auto-play started");
            }
#else
            if (!SceneSerializer::s_cinematicPaths.empty()) {
                Logger::Info("[CinematicManager] {} sequence(s) loaded", SceneSerializer::s_cinematicPaths.size());
            }
#endif
            cinematicsLoaded_ = true;
        }
    }

    // シネマティック更新（再生中のカメラ上書き + 完了コールバック）
    cinematicManager_.Update(deltaTime);

    // 再生終了後、Enterキーでintroリプレイ
    auto* input = GetInput();

    // WaitForInput中にEnter/Spaceで再開
    if (cinematicManager_.GetPlayer().IsWaitingForInput() && input) {
        if (input->GetKeyboard().IsPressed(KeyCode::Enter) ||
            input->GetKeyboard().IsPressed(KeyCode::Space)) {
            cinematicManager_.GetPlayer().ResolveWaitForInput();
        }
    }
    if (input && cinematicsLoaded_ && cinematicManager_.IsFinished()) {
        if (input->GetKeyboard().IsPressed(KeyCode::Enter)) {
            cinematicManager_.Play("intro");
            Logger::Info("[CinematicManager] Replay started");
        }
    }

#ifndef WITH_EDITOR
    // ESCキー1.5秒長押しでゲーム終了
    if (input && input->GetKeyboard().IsDown(KeyCode::Escape)) {
        escHoldTime_ += deltaTime;
        if (escHoldTime_ >= kEscQuitThreshold) {
            PostQuitMessage(0);
        }
    } else {
        escHoldTime_ = 0.0f;
    }
#endif
}

Mesh* GameApplication::LoadMesh(const std::string& path) {
    return ResourceLoader::LoadMesh(path);
}

Material* GameApplication::LoadMaterial(const std::string& name) {
    return ResourceLoader::LoadMaterial(name);
}

#ifdef WITH_EDITOR
void GameApplication::OnLoadingPhase() {
    Scene* scene = GetSceneManager()->GetActiveScene();
    auto* editorUI = scene ? scene->GetEditorUI() : nullptr;
    if (!editorUI) return;

    // 全モデルのサムネイルをキューに積む
    editorUI->QueueAllThumbnails();

    size_t totalThumbnails = editorUI->GetThumbnailTotalCount();
    if (totalThumbnails == 0) return;

    Logger::Info("[ローディング] サムネイル生成開始: {}個", totalThumbnails);

    // ── Phase 1: バックグラウンドスレッドでモデルデータを一括プリロード ──
    std::atomic<int> preloadedCount{0};
    std::atomic<bool> preloadDone{false};

    std::thread preloadThread([&]() {
        editorUI->PreLoadAllThumbnailsAsync(preloadedCount);
        preloadDone.store(true, std::memory_order_release);
    });

    float displayProgress = 0.0f;

    // メインスレッド: スムーズなローディング画面を描画
    while (!preloadDone.load(std::memory_order_acquire)) {
        if (!GetWindow()->ProcessMessages()) {
            preloadThread.join();
            return;
        }

        // ターゲット進捗（プリロードは全体の50%）
        int loaded = preloadedCount.load(std::memory_order_relaxed);
        float targetProgress = static_cast<float>(loaded) / static_cast<float>(totalThumbnails) * 0.5f;

        // イージング（Exponential ease-out）
        displayProgress += (targetProgress - displayProgress) * 0.12f;
        if (std::abs(targetProgress - displayProgress) < 0.001f) {
            displayProgress = targetProgress;
        }

        std::string msg = "Loading assets... (" + std::to_string(loaded) + "/" + std::to_string(totalThumbnails) + ")";

        graphics_->BeginFrame();
        renderer_->BeginFrame();
        graphics_->SetBackBufferAsRenderTarget();
        renderer_->RenderLoadingScreen(msg, displayProgress);
        graphics_->EndFrame();
        graphics_->Present();
    }

    preloadThread.join();

    // ── Phase 2: サムネイル描画（モデルはキャッシュ済みなので高速） ──
    while (editorUI->HasPendingThumbnails()) {
        if (!GetWindow()->ProcessMessages()) break;

        size_t pending = editorUI->GetThumbnailPendingCount();
        size_t done = totalThumbnails - pending;
        float targetProgress = 0.5f + static_cast<float>(done) / static_cast<float>(totalThumbnails) * 0.5f;

        // イージング
        displayProgress += (targetProgress - displayProgress) * 0.18f;
        if (std::abs(targetProgress - displayProgress) < 0.001f) {
            displayProgress = targetProgress;
        }

        std::string msg = "Generating previews... (" + std::to_string(done) + "/" + std::to_string(totalThumbnails) + ")";

        // PreLoadPendingはスキップ（Phase 1で全モデルキャッシュ済み）
        graphics_->BeginFrame();
        renderer_->BeginFrame();
        editorUI->ProcessPendingThumbnails();
        graphics_->SetBackBufferAsRenderTarget();
        renderer_->RenderLoadingScreen(msg, displayProgress);
        graphics_->EndFrame();
        graphics_->Present();
    }

    // イージング完了まで数フレーム描画
    for (int f = 0; f < 15 && displayProgress < 0.99f; f++) {
        if (!GetWindow()->ProcessMessages()) break;
        displayProgress += (1.0f - displayProgress) * 0.25f;
        graphics_->BeginFrame();
        renderer_->BeginFrame();
        graphics_->SetBackBufferAsRenderTarget();
        renderer_->RenderLoadingScreen("Ready!", displayProgress);
        graphics_->EndFrame();
        graphics_->Present();
    }

    Logger::Info("[ローディング] サムネイル生成完了");
}
#endif

void GameApplication::OnRender() {
#ifdef WITH_EDITOR
    // Phase 1: BeginFrame前にサムネイル用モデルをキャッシュへロード
    // (ResourceLoader::LoadModel は内部で commandList->Reset するため BeginFrame前に行う)
    {
        Scene* scene = GetSceneManager()->GetActiveScene();
        if (auto* editorUI = scene ? scene->GetEditorUI() : nullptr) {
            editorUI->PreLoadPendingThumbnails();
        }
    }
#endif

    graphics_->BeginFrame();
    renderer_->BeginFrame();

    Scene* scene = GetSceneManager()->GetActiveScene();

    // Sync all light components from scene to LightManager every frame
    if (scene && lightManager_) {
        lightManager_->SyncFromScene(scene->GetGameObjects());
    }

    if (scene) {
        // ビデオフレームをGPUにアップロード（コマンドリストがオープンな状態で実行）
        auto* cmdList = graphics_->GetCommandList();
        for (auto& obj : scene->GetGameObjects()) {
            if (auto* videoPlayer = obj->GetComponent<VideoPlayerComponent>()) {
                if (videoPlayer->HasPendingFrame()) {
                    videoPlayer->UploadVideoFrame(cmdList);
                }
            }
        }
    }

    if (scene) {
        RenderView view;
        scene->OnRender(view);

        // Main Cameraを持つGameObjectからCameraComponentを探す
        CameraComponent* camComp = nullptr;
        for (auto& obj : scene->GetGameObjects()) {
            auto* cc = obj->GetComponent<CameraComponent>();
            if (cc && cc->IsMain()) {
                camComp = cc;
                break;
            }
        }

        // 一人称視点でターゲットモデルを除外する設定
        if (camComp) {
            view.excludeFromFirstPerson = camComp->GetFirstPersonExcludeTarget();
        }

        // Collect render items via RenderSystem
        const auto& items = renderSystem_->CollectRenderables(scene, view);
        const auto& skinnedItems = renderSystem_->CollectSkinnedRenderables(scene, view);
        
        static bool loggedOnce = false;
        if (!loggedOnce) {
            Logger::Info("[描画] スキンメッシュ {}個 収集完了", skinnedItems.size());
            loggedOnce = true;
        }

#ifdef WITH_EDITOR
        auto* editorUI = scene->GetEditorUI();
        if (editorUI) {
            // サムネイルを1フレームに1枚処理（メインレンダー前）
            editorUI->ProcessPendingThumbnails();

            auto* debugRenderer = renderer_->GetDebugRenderer();

            // Scene View用カメラを取得（Main Cameraとは完全に独立したEditorCamera）
            Camera* sceneCamera = editorUI->GetSceneViewCamera();

            // デバッグ: カメラが異なることを確認
            if (sceneCamera == view.camera) {
                Logger::Warning("[描画] SceneCameraとMainCameraが同じです！");
            }

            // シャドウマップを1回だけ描画（Game View + Scene Viewで共有）
            renderer_->RenderShadowPrePass(view, items, skinnedItems, lightManager_.get());

            // Game Viewに描画（Main Cameraを使用）
            auto* gameViewTex = editorUI->GetGameViewTexture();
            if (gameViewTex && gameViewTex->GetResource() && view.camera) {
                renderer_->DrawToTexture(
                    gameViewTex->GetResource(),
                    gameViewTex->GetRTVHandle(),
                    gameViewTex->GetDSVHandle(),
                    view,  // Main Camera
                    items,
                    lightManager_.get(),
                    skinnedItems,
                    false,  // デバッグ描画無効
                    {},
                    {},
                    true   // シャドウ済み
                );

                // ポストプロセス設定を取得して適用
                if (camComp && camComp->IsPostProcessEnabled() && 
                    !camComp->GetPostProcessEffects().empty()) {
                    auto* postProcessMgr = editorUI->GetPostProcessManager();
                    auto* postProcessOutput = editorUI->GetPostProcessOutputTexture();
                    if (postProcessMgr && postProcessOutput) {
                        postProcessMgr->SetEffectChain(camComp->GetPostProcessEffects());
                        postProcessMgr->Apply(graphics_.get(), gameViewTex, postProcessOutput);
                    }
                } else {
                    auto* postProcessMgr = editorUI->GetPostProcessManager();
                    if (postProcessMgr) {
                        postProcessMgr->ClearEffects();
                    }
                }
            }

            // Scene Viewに描画（EditorCameraを使用）
            auto* sceneViewTex = editorUI->GetSceneViewTexture();
            if (sceneViewTex && sceneViewTex->GetResource() && sceneCamera) {
                // デバッグ描画の準備
                if (debugRenderer) {
                    debugRenderer->BeginFrame();
                    editorUI->PrepareSceneViewGizmos(debugRenderer);
                }

                // Build outline items for the selected object (edit mode only)
                std::vector<RenderItem> outlineItems;
                std::vector<SkinnedRenderItem> outlineSkinnedItems;
                if (auto* sel = editorUI->GetSelectedObject(); sel && editorUI->IsEditing()) {
                    // Static meshes: use world matrix directly
                    if (auto* mr = sel->GetComponent<MeshRenderer>(); mr && mr->HasModel()) {
                        const Matrix4x4 worldMat = sel->GetTransform().GetWorldMatrix();
                        for (const auto& mesh : mr->GetMeshes()) {
                            outlineItems.emplace_back(const_cast<Mesh*>(&mesh),
                                                      const_cast<Material*>(mesh.GetMaterial()),
                                                      worldMat);
                        }
                    }
                    // Skinned meshes: find in skinnedItems to reuse the exact world matrix
                    // (CollectSkinnedRenderables applies coordinate corrections)
                    if (auto* smr = sel->GetComponent<SkinnedMeshRenderer>(); smr && smr->HasModel()) {
                        const auto& selMeshes = smr->GetMeshes();
                        for (const auto& si : skinnedItems) {
                            for (const auto& mesh : selMeshes) {
                                if (si.mesh == &mesh) {
                                    outlineSkinnedItems.push_back(si);
                                    break;
                                }
                            }
                        }
                    }
                }

                // Scene View用のRenderViewを作成
                RenderView sceneView;
                sceneView.camera = sceneCamera;  // EditorCamera（sceneViewCamera_）
                sceneView.layerMask = view.layerMask;
                sceneView.viewName = "SceneView";

                renderer_->DrawToTexture(
                    sceneViewTex->GetResource(),
                    sceneViewTex->GetRTVHandle(),
                    sceneViewTex->GetDSVHandle(),
                    sceneView,
                    items,
                    lightManager_.get(),
                    skinnedItems,
                    true,  // デバッグ描画有効
                    outlineItems,
                    outlineSkinnedItems,
                    true   // シャドウ済み
                );
            }

            // シャドウマップをDEPTH_WRITEに復元
            renderer_->RestoreShadowMaps();

            // メインウィンドウのレンダーターゲットを再設定
            graphics_->SetBackBufferAsRenderTarget();

            // UIのみ描画
            renderer_->RenderUIOnly(scene);
        }
#else
        // Release: Draw directly to back buffer
        graphics_->SetBackBufferAsRenderTarget();
        renderer_->Draw(view, items, lightManager_.get(), scene);
        if (!skinnedItems.empty()) {
            renderer_->DrawSkinnedMeshes(view, skinnedItems, lightManager_.get());
        }
#endif
    }

    graphics_->EndFrame();
    graphics_->Present();
}

} // namespace UnoEngine
