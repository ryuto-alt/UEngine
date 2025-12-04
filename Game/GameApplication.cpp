#include "pch.h"
#include "GameApplication.h"
#include "../Engine/Core/Scene.h"
#include "../Engine/Resource/ResourceLoader.h"
#include "../Engine/Rendering/RenderSystem.h"
#include "../Engine/Rendering/SkinnedRenderItem.h"
#include "../Engine/Audio/AudioSystem.h"
#include "../Engine/Core/Logger.h"

namespace UnoEngine {

void GameApplication::OnInit() {
    // Initialize ResourceManager
    resourceManager_ = std::make_unique<ResourceManager>(graphics_.get());
    Logger::Info("[初期化] ResourceManager 準備完了");

    // Register systems
    GetSystemManager()->RegisterSystem<AnimationSystem>();
    GetSystemManager()->RegisterSystem<CameraSystem>();
    GetSystemManager()->RegisterSystem<AudioSystem>();
    Logger::Info("[初期化] システム登録完了 (Animation, Camera, Audio)");
}

Mesh* GameApplication::LoadMesh(const std::string& path) {
    return ResourceLoader::LoadMesh(path);
}

Material* GameApplication::LoadMaterial(const std::string& name) {
    return ResourceLoader::LoadMaterial(name);
}

void GameApplication::OnRender() {
    graphics_->BeginFrame();
    renderer_->BeginFrame();  // ダイナミックバッファをリセット

    Scene* scene = GetSceneManager()->GetActiveScene();
    if (scene) {
        RenderView view;
        scene->OnRender(view);

        // Collect render items via RenderSystem
        auto items = renderSystem_->CollectRenderables(scene, view);
        auto skinnedItems = renderSystem_->CollectSkinnedRenderables(scene, view);
        
        static bool loggedOnce = false;
        if (!loggedOnce) {
            Logger::Info("[描画] スキンメッシュ {}個 収集完了", skinnedItems.size());
            loggedOnce = true;
        }

#ifdef _DEBUG
        auto* editorUI = scene->GetEditorUI();
        if (editorUI) {
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
            }

            // Scene Viewに描画（EditorCameraを使用）
            auto* sceneViewTex = editorUI->GetSceneViewTexture();
            if (sceneViewTex && sceneViewTex->GetResource() && sceneCamera) {
                // デバッグ描画の準備
                if (debugRenderer) {
                    debugRenderer->BeginFrame();
                    editorUI->PrepareSceneViewGizmos(debugRenderer);
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
                    true  // デバッグ描画有効
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
