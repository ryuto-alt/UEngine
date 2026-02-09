#include "AudioListenerSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/AudioComponents.h"
#include "UnoEngine.h"
#include <cmath>

namespace ECS {

void AudioListenerSystem::Update(World& world, float deltaTime) {
    Camera* camera = world.GetResource<Camera*>();

    world.ForEach<PlayerTag, TransformComponent, AudioListenerComponent>(
        [camera](Entity entity, PlayerTag&, TransformComponent& transform,
                AudioListenerComponent& listener) {
            if (!listener.listener) return;

            listener.listener->SetPosition(transform.position);

            if (camera) {
                Vector3 cameraRot = camera->GetRotate();
                Vector3 forward = {
                    std::sin(cameraRot.y),
                    0.0f,
                    std::cos(cameraRot.y)
                };
                listener.listener->SetOrientation(forward, Vector3{0.0f, 1.0f, 0.0f});
            }
        }
    );
}

} // namespace ECS
