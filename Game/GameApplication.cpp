#include "pch.h"
#include "GameApplication.h"
#include "../Engine/Core/Scene.h"
#include "../Engine/Resource/ResourceLoader.h"
#include "../Engine/Rendering/RenderSystem.h"
#include "../Engine/Rendering/SkinnedRenderItem.h"
#include "../Engine/Audio/AudioSystem.h"
#include "../Engine/Systems/CollisionSystem.h"
#include "../Engine/Systems/PhysicsSystem.h"
#include "../Engine/Core/Logger.h"
#include "../Engine/Video/VideoPlayerComponent.h"
#ifdef WITH_EDITOR
#include "../Engine/Graphics/MeshRenderer.h"
#include "../Engine/Rendering/SkinnedMeshRenderer.h"
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
    Logger::Info("[初期化] システム登録完了 (Animation, Camera, Audio, Collision, Physics)");
}

Mesh* GameApplication::LoadMesh(const std::string& path) {
    return ResourceLoader::LoadMesh(path);
}

Material* GameApplication::LoadMaterial(const std::string& name) {
    return ResourceLoader::LoadMaterial(name);
}

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
        auto items = renderSystem_->CollectRenderables(scene, view);
        auto skinnedItems = renderSystem_->CollectSkinnedRenderables(scene, view);
        
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
                    false  // デバッグ描画無効
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
                    outlineSkinnedItems
                );
            }

            // メインウィンドウのレンダーターゲットを再設定
            graphics_->SetBackBufferAsRenderTarget();

            // UIのみ描画
            renderer_->RenderUIOnly(scene);
        }
#else
        // Release: Draw directly to back buffer
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
