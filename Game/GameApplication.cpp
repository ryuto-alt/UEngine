#include "GameApplication.h"
#include "Scenes/GameScene.h"
#include "../Engine/Resource/ResourceLoader.h"
#include "../Engine/Rendering/RenderSystem.h"
#include "../Engine/Rendering/SkinnedRenderItem.h"
#include "../Engine/Core/Logger.h"

namespace UnoEngine {

void GameApplication::OnInit() {
    // Initialize ResourceManager
    resourceManager_ = std::make_unique<ResourceManager>(graphics_.get());
    Logger::Info("GameApplication: ResourceManager initialized");

    // Register systems
    GetSystemManager()->RegisterSystem<AnimationSystem>();
    GetSystemManager()->RegisterSystem<PlayerSystem>();
}

Mesh* GameApplication::LoadMesh(const std::string& path) {
    return ResourceLoader::LoadMesh(path);
}

Material* GameApplication::LoadMaterial(const std::string& name) {
    return ResourceLoader::LoadMaterial(name);
}

void GameApplication::OnRender() {
    graphics_->BeginFrame();

    Scene* scene = GetSceneManager()->GetActiveScene();
    if (scene) {
        RenderView view;
        scene->OnRender(view);

        // Collect render items via RenderSystem
        auto items = renderSystem_->CollectRenderables(scene, view);
        auto skinnedItems = renderSystem_->CollectSkinnedRenderables(scene, view);
        
        static bool loggedOnce = false;
        if (!loggedOnce) {
            Logger::Info("GameApplication::OnRender: skinnedItems.size() = {}", skinnedItems.size());
            loggedOnce = true;
        }

#ifdef _DEBUG
        GameScene* gameScene = dynamic_cast<GameScene*>(scene);
        if (gameScene) {
            auto* editorUI = gameScene->GetEditorUI();

            // Game Viewに描画
            auto* gameViewTex = editorUI->GetGameViewTexture();
            if (gameViewTex && gameViewTex->GetResource()) {
                renderer_->DrawToTexture(
                    gameViewTex->GetResource(),
                    gameViewTex->GetRTVHandle(),
                    gameViewTex->GetDSVHandle(),
                    view,
                    items,
                    lightManager_.get(),
                    skinnedItems
                );
            }

            // Scene Viewに描画
            auto* sceneViewTex = editorUI->GetSceneViewTexture();
            if (sceneViewTex && sceneViewTex->GetResource()) {
                renderer_->DrawToTexture(
                    sceneViewTex->GetResource(),
                    sceneViewTex->GetRTVHandle(),
                    sceneViewTex->GetDSVHandle(),
                    view,
                    items,
                    lightManager_.get(),
                    skinnedItems
                );
            }

            // メインウィンドウのレンダーターゲットを再設定
            graphics_->SetBackBufferAsRenderTarget();

            // UIのみ描画
            renderer_->RenderUIOnly(scene);
        }
#else
        // Release: Draw directly to back buffer
        renderer_->Draw(view, items, lightManager_.get(), scene, skinnedItems);
#endif
    }

    graphics_->EndFrame();
    graphics_->Present();
}

} // namespace UnoEngine
