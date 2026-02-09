#include "SpotLightGlowSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/CollectibleComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "Object3d.h"
#include "Manager/LightManager.h"
#include <cmath>

namespace ECS {

void SpotLightGlowSystem::Update(World& world, float deltaTime) {
    auto* lightMgr = world.TryGetResource<LightManager*>();
    if (!lightMgr || !(*lightMgr)) return;

    const SpotLight& spotLight = (*lightMgr)->GetSpotLight();

    world.ForEach<TransformComponent, SpotLightGlowComponent, OrbComponent, MeshRendererComponent>(
        [&spotLight](Entity entity, TransformComponent& transform, SpotLightGlowComponent& glow,
                     OrbComponent& orb, MeshRendererComponent& renderer) {
            if (orb.isCollected) return;

            glow.currentSpotLight = &spotLight;

            // Vector from orb to spotlight
            Vector3 toLight = {
                spotLight.position.x - transform.position.x,
                spotLight.position.y - transform.position.y,
                spotLight.position.z - transform.position.z
            };

            float distance = std::sqrt(toLight.x * toLight.x + toLight.y * toLight.y + toLight.z * toLight.z);

            if (distance > 0.0001f) {
                toLight.x /= distance;
                toLight.y /= distance;
                toLight.z /= distance;
            }

            // Dot product with spotlight direction (cone check)
            float dotProduct = -(toLight.x * spotLight.direction.x +
                                toLight.y * spotLight.direction.y +
                                toLight.z * spotLight.direction.z);

            if (dotProduct > spotLight.outerCone) {
                float attenuation = 1.0f / (
                    spotLight.attenuation.x +
                    spotLight.attenuation.y * distance +
                    spotLight.attenuation.z * distance * distance
                );

                float spotIntensity = 1.0f;
                if (dotProduct < spotLight.innerCone) {
                    float epsilon = spotLight.innerCone - spotLight.outerCone;
                    if (epsilon > 0.0001f) {
                        spotIntensity = (dotProduct - spotLight.outerCone) / epsilon;
                        spotIntensity = std::clamp(spotIntensity, 0.0f, 1.0f);
                    }
                }

                glow.glowIntensity = spotLight.intensity * attenuation * spotIntensity;
                glow.isIlluminated = (glow.glowIntensity > 0.1f);
            } else {
                glow.isIlluminated = false;
                glow.glowIntensity = 0.0f;
            }

            // Apply glow color to Object3d
            if (renderer.object3d) {
                if (glow.isIlluminated) {
                    float intensity = 1.0f + glow.glowIntensity * 2.0f;
                    Vector4 glowColor = {
                        1.0f * intensity,
                        0.95f * intensity,
                        0.6f * intensity,
                        1.0f
                    };
                    renderer.object3d->SetColor(glowColor);
                } else {
                    renderer.object3d->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
                }
            }
        }
    );
}

} // namespace ECS
