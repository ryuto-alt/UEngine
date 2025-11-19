#include "GameScene.h"
#include "../Components/Player.h"
#include "../GameApplication.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Graphics/MeshRenderer.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Math/Math.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace UnoEngine {

void GameScene::OnLoad() {
    SetActiveCamera(nullptr);

    // Camera setup
    auto camera = MakeUnique<Camera>();
    camera->SetPosition(Vector3(0.0f, 5.0f, 10.0f));
    camera->SetRotation(Quaternion::LookRotation(Vector3(0.0f, -0.5f, -1.0f).Normalize(), Vector3::UnitY()));
    SetActiveCamera(camera.release());

    // Player setup
    player_ = CreateGameObject("Player");
    auto* app = static_cast<GameApplication*>(GetApplication());
    auto* mesh = app->LoadMesh("resources/model/testmodel/testmodel.obj");
    player_->AddComponent<MeshRenderer>(mesh, const_cast<Material*>(mesh->GetMaterial()));
    player_->AddComponent<Player>();

    // Light setup
    auto* light = CreateGameObject("DirectionalLight");
    auto* lightComp = light->AddComponent<DirectionalLightComponent>();
    lightComp->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
    lightComp->SetColor(Vector3(1.0f, 1.0f, 1.0f));
    lightComp->SetIntensity(1.0f);
    lightComp->UseTransformDirection(false);

#ifdef _DEBUG
    // EditorUI初期化 (Debug builds only)
    editorUI_.Initialize(app->GetGraphicsDevice());
    editorUI_.AddConsoleMessage("[Scene] GameScene loaded successfully");
#endif
}

void GameScene::OnUpdate(float deltaTime) {
#ifdef _DEBUG
    // 前のフレームで記録したサイズでRenderTextureをリサイズ（描画前に実行）
    auto* app = static_cast<GameApplication*>(GetApplication());
    uint32 gameW, gameH, sceneW, sceneH;
    editorUI_.GetDesiredViewportSizes(gameW, gameH, sceneW, sceneH);
    editorUI_.GetGameViewTexture()->Resize(app->GetGraphicsDevice(), gameW, gameH);
    editorUI_.GetSceneViewTexture()->Resize(app->GetGraphicsDevice(), sceneW, sceneH);
#endif

    // Rotate player
    if (player_) {
        auto& transform = player_->GetTransform();
        Quaternion currentRotation = transform.GetLocalRotation();
        Quaternion deltaRotation = Quaternion::RotationAxis(Vector3::UnitY(), Math::ToRadians(30.0f) * deltaTime);
        transform.SetLocalRotation(deltaRotation * currentRotation);
    }

    // Update camera input via player system
    Camera* camera = GetActiveCamera();
    if (camera && player_) {
        auto* playerComp = player_->GetComponent<Player>();
        auto* app = static_cast<GameApplication*>(GetApplication());
        app->GetPlayerSystem().Update(camera, playerComp, input_, deltaTime);
    }
}

void GameScene::OnImGui() {
#ifdef _DEBUG
    // EditorContextを構築
    EditorContext context;
    context.player = player_;
    context.camera = GetActiveCamera();
    context.gameObjects = &GetGameObjects();
    context.fps = ImGui::GetIO().Framerate;
    context.frameTime = 1000.0f / ImGui::GetIO().Framerate;

    // EditorUIに描画を委譲
    editorUI_.Render(context);
#endif
}

void GameScene::OnRender(RenderView& view) {
    Camera* camera = GetActiveCamera();
    if (!camera) return;
    
    view.camera = camera;
    view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
    view.viewName = "MainView";


}

} // namespace UnoEngine
