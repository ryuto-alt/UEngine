#include "GameApplication.h"
#include "Scenes/GameScene.h"
#include "../Engine/Resource/ResourceLoader.h"
#include "../Engine/Rendering/RenderSystem.h"
#include "../Engine/Rendering/SkinnedRenderItem.h"

namespace UnoEngine {

void GameApplication::OnInit() {
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

        auto items = renderSystem_->CollectRenderables(scene, view);

        // スキンメッシュアイテムを取得
        std::vector<SkinnedRenderItem> skinnedItems;
        GameScene* gameScene = dynamic_cast<GameScene*>(scene);
        if (gameScene) {
            skinnedItems = gameScene->GetSkinnedRenderItems();
        }

#ifdef _DEBUG
        // GameSceneの場合はRenderTextureに描画 (Debug builds only)
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

            // UIのみ描画（メッシュは描画しない）
            renderer_->RenderUIOnly(scene);
        }
#endif
    }

    graphics_->EndFrame();
    graphics_->Present();
}

} // namespace UnoEngine
