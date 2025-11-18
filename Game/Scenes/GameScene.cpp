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
    // Camera
    auto* camera = CreateGameObject("Camera");
    camera->AddComponent<OrbitController>();

    // Player
    auto* player = CreateGameObject("Player");
    player->AddComponent<MeshRenderer>(
        ResourceLoader::LoadMesh("resources/model/testmodel/testmodel.obj"),
        ResourceLoader::LoadMaterial("default")
    );
    player->AddComponent<PlayerController>();

    // Directional Light
    auto* light = CreateGameObject("DirectionalLight");
    light->GetTransform().SetLocalRotation(
        Quaternion::RotationAxis(Vector3::UnitX(), Math::ToRadians(90.0f))
    );
    auto* lightComp = light->AddComponent<DirectionalLightComponent>();
    lightComp->SetColor(Vector3(1.0f, 1.0f, 1.0f));
    lightComp->SetIntensity(2.0f);
    lightComp->UseTransformDirection(true);
}

} // namespace UnoEngine
