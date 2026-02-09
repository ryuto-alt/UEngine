#include "DebugInputSystem.h"
#include "ECS/World.h"
#include "ECS/Components/CameraComponents.h"
#include "ECS/Components/PostProcessComponents.h"
#include "UnoEngine.h"
#include "GameObject/FPSCamera.h"

namespace ECS {

void DebugInputSystem::Update(World& world, float deltaTime) {
    UnoEngine* engine = UnoEngine::GetInstance();

    // V key: toggle FPS/third-person camera
    if (engine->IsKeyTrig(DIK_V)) {
        world.ForEach<FPSCameraComponent>(
            [](Entity entity, FPSCameraComponent& fpsCam) {
                if (fpsCam.fpsCamera) {
                    fpsCam.isFPSMode = !fpsCam.isFPSMode;
                    fpsCam.fpsCamera->SetFPSMode(fpsCam.isFPSMode);
                }
            }
        );
    }

    // TAB key: toggle mouse look
    if (engine->IsKeyTrig(DIK_TAB)) {
        world.ForEach<FPSCameraComponent>(
            [](Entity entity, FPSCameraComponent& fpsCam) {
                if (fpsCam.fpsCamera) {
                    fpsCam.fpsCamera->ToggleMouseLook();
                }
            }
        );
    }

    // F4 key: toggle fisheye lens
    if (engine->IsKeyTrig(DIK_F4)) {
        world.ForEach<PostProcessChainComponent>(
            [](Entity entity, PostProcessChainComponent& pp) {
                if (pp.fisheyeStrength > 0.0f) {
                    pp.fisheyeStrength = 0.0f;
                } else {
                    pp.fisheyeStrength = 2.58f;
                }
            }
        );
    }
}

} // namespace ECS
