#include "GameApplication.h"
#include "Scenes/GameScene.h"
#include "../Engine/Resource/ResourceLoader.h"
#include "../Engine/Rendering/RenderSystem.h"

namespace UnoEngine {

void GameApplication::OnInit() {
    // Game-specific initialization
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
        // GameSceneの場合はRenderTextureに描画
        GameScene* gameScene = dynamic_cast<GameScene*>(scene);
        
        RenderView view;
        scene->OnRender(view);
        
        auto items = renderSystem_->CollectRenderables(scene, view);

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
                lightManager_.get()
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
                lightManager_.get()
            );
            }
        }

        // メインウィンドウのレンダーターゲットを再設定
        if (gameScene) {
            graphics_->SetBackBufferAsRenderTarget();
        }

        // メインウィンドウに描画
        renderer_->Draw(view, items, lightManager_.get(), scene);
    }
    
    graphics_->EndFrame();
    graphics_->Present();
}

} // namespace UnoEngine
