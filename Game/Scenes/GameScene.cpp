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

    // Player setup (glTF model)
    player_ = CreateGameObject("Player");
    auto* app = static_cast<GameApplication*>(GetApplication());
    auto* mesh = app->LoadMesh("assets/model/testmodel/walk.gltf");
    player_->AddComponent<MeshRenderer>(mesh, const_cast<Material*>(mesh->GetMaterial()));
    player_->AddComponent<Player>();

    // Main Light (斜め前から)
    auto* mainLight = CreateGameObject("MainLight");
    auto* mainLightComp = mainLight->AddComponent<DirectionalLightComponent>();
    mainLightComp->SetDirection(Vector3(0.3f, -0.7f, -0.5f).Normalize());
    mainLightComp->SetColor(Vector3(1.0f, 0.98f, 0.95f)); // 少し暖色
    mainLightComp->SetIntensity(1.2f);
    mainLightComp->UseTransformDirection(false);

    // Rim Light (背後から、輪郭を強調)
    auto* rimLight = CreateGameObject("RimLight");
    auto* rimLightComp = rimLight->AddComponent<DirectionalLightComponent>();
    rimLightComp->SetDirection(Vector3(-0.2f, 0.3f, 0.8f).Normalize());
    rimLightComp->SetColor(Vector3(0.7f, 0.8f, 1.0f)); // 少し寒色
    rimLightComp->SetIntensity(0.5f);
    rimLightComp->UseTransformDirection(false);

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
