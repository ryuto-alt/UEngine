#include "GameScene.h"
#include "../Components/Player.h"
#include "../GameApplication.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Graphics/MeshRenderer.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Math/Math.h"
#include "../../Engine/UI/ImGuiManager.h"
#include <imgui.h>

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
}

void GameScene::OnUpdate(float deltaTime) {
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
    ImGui::Begin("Debug Info");

    Camera* camera = GetActiveCamera();
    if (camera) {
        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
            camera->GetPosition().GetX(),
            camera->GetPosition().GetY(),
            camera->GetPosition().GetZ());
    }

    if (player_) {
        auto pos = player_->GetTransform().GetLocalPosition();
        ImGui::Text("Player Position: (%.2f, %.2f, %.2f)", pos.GetX(), pos.GetY(), pos.GetZ());
    }

    ImGui::End();
}

void GameScene::OnRender(RenderView& view) {
    Camera* camera = GetActiveCamera();
    if (!camera) return;
    
    view.camera = camera;
    view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
    view.viewName = "MainView";


}

} // namespace UnoEngine
