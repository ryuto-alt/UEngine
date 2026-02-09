#include "TransformSyncSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "Object3d.h"

namespace ECS {

void TransformSyncSystem::Update(World& world, float deltaTime) {
    world.ForEach<TransformComponent, MeshRendererComponent>(
        [](Entity entity, TransformComponent& transform, MeshRendererComponent& renderer) {
            if (!renderer.object3d) return;

            renderer.object3d->SetPosition(transform.position);
            renderer.object3d->SetRotation(transform.rotation);
            renderer.object3d->SetScale(transform.scale);
            renderer.object3d->Update();
        }
    );
}

} // namespace ECS
