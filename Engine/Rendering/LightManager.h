#pragma once

#include "../Graphics/DirectionalLight.h"
#include "../Graphics/PointLightComponent.h"
#include "../Graphics/SpotLightComponent.h"
#include "../Core/Types.h"
#include <array>
#include <vector>

namespace UnoEngine {

class DirectionalLightComponent;
class GameObject;

struct GPULightData {
    static constexpr uint32 kMaxPointLights = 8;
    static constexpr uint32 kMaxSpotLights  = 4;

    Vector3 direction{0.0f, -1.0f, 0.0f};
    Vector3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    Vector3 ambient{0.3f, 0.3f, 0.3f};

    struct PointLight {
        Vector3 position;
        float range;
        Vector3 color;
        float intensity;
    };

    struct SpotLight {
        Vector3 position;
        float range;
        Vector3 direction;
        float spotAngle;
        Vector3 color;
        float intensity;
        float innerAngle;
    };

    std::array<PointLight, kMaxPointLights> pointLights{};
    uint32 pointLightCount = 0;
    std::array<SpotLight, kMaxSpotLights>   spotLights{};
    uint32 spotLightCount = 0;
};

class LightManager {
public:
    LightManager() = default;
    ~LightManager() = default;

    void RegisterLight(DirectionalLightComponent* light);
    void UnregisterLight(DirectionalLightComponent* light);

    void RegisterLight(PointLightComponent* light);
    void UnregisterLight(PointLightComponent* light);

    void RegisterLight(SpotLightComponent* light);
    void UnregisterLight(SpotLightComponent* light);

    void Clear();

    // Sync lights from scene objects (call every frame before render)
    void SyncFromScene(const std::vector<UniquePtr<GameObject>>& objects);

    GPULightData BuildGPULightData() const;

    DirectionalLightComponent* GetDirectionalLight() const;

private:
    DirectionalLightComponent*        directionalLight_ = nullptr;
    std::vector<PointLightComponent*> pointLights_;
    std::vector<SpotLightComponent*>  spotLights_;
};

} // namespace UnoEngine
