#include "CullingSystemECS.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "UnoEngine.h"

namespace ECS {

void CullingSystemECS::Update(World& world, float deltaTime) {
    // Culling is handled by Object3d::Draw with camera frustum internally
    // This system can be extended for custom distance-based culling

    Camera* camera = world.GetResource<Camera*>();
    if (!camera) return;

    Vector3 camPos = camera->GetTranslate();

    world.ForEach<TransformComponent, MeshRendererComponent>(
        [&camPos](Entity entity, TransformComponent& transform, MeshRendererComponent& renderer) {
            if (!renderer.object3d) return;

            // Distance culling: hide objects beyond 200m
            float dx = transform.position.x - camPos.x;
            float dz = transform.position.z - camPos.z;
            float distSq = dx * dx + dz * dz;

            renderer.visible = distSq < (200.0f * 200.0f);
        }
    );
}

} // namespace ECS
