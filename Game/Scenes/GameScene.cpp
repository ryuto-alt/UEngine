#include "GameScene.h"
#include "../Components/PlayerController.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/OrbitController.h"
#include "../../Engine/Graphics/ResourceLoader.h"
#include "../../Engine/Graphics/MeshRenderer.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Math/Math.h"

namespace UnoEngine {

void GameScene::OnLoad() {
    SetActiveCamera(nullptr);
    
    // Camera
    auto* camera = CreateGameObject("Camera");
    auto* orbitController = camera->AddComponent<OrbitController>();
    if (input_) {
        orbitController->SetInputManager(input_);
    }

    // Player
    auto* player = CreateGameObject("Player");
    player->AddComponent<MeshRenderer>(
        ResourceLoader::LoadMesh("resources/model/testmodel/testmodel.obj"),
        ResourceLoader::LoadMaterial("default")
    );
    auto* playerController = player->AddComponent<PlayerController>();
    if (input_) {
        playerController->SetInputManager(input_);
    }

    // Directional Light
    auto* light = CreateGameObject("DirectionalLight");
    light->GetTransform().SetLocalRotation(
        Quaternion::RotationAxis(Vector3::UnitX(), Math::ToRadians(90.0f))
    );
    auto* lightComp = light->AddComponent<DirectionalLightComponent>();
    lightComp->SetColor(Vector3(1.0f, 1.0f, 1.0f));
    lightComp->SetIntensity(2.0f);
    lightComp->UseTransformDirection(true);
    
    // Cameraを検索してactiveに設定
    for (const auto& go : GetGameObjects()) {
        if (go->GetName() == "Camera") {
            auto* orbitController = go->GetComponent<OrbitController>();
            if (orbitController) {
                SetActiveCamera(orbitController->GetCamera());
                break;
            }
        }
    }
}

void GameScene::OnUpdate(float deltaTime) {
    // Playerオブジェクトを回転
    for (const auto& go : GetGameObjects()) {
        if (go->GetName() == "Player") {
            auto& transform = go->GetTransform();
            Quaternion currentRotation = transform.GetLocalRotation();
            Quaternion deltaRotation = Quaternion::RotationAxis(Vector3::UnitY(), Math::ToRadians(30.0f) * deltaTime);
            transform.SetLocalRotation(deltaRotation * currentRotation);
            break;
        }
    }
}

void GameScene::OnRender(RenderView& view) {
    Camera* camera = GetActiveCamera();
    if (!camera) return;
    
    view.camera = camera;
    view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
    view.viewName = "MainView";
}

} // namespace UnoEngine
