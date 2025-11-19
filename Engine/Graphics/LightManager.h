#pragma once

#include "DirectionalLight.h"

namespace UnoEngine {

class DirectionalLightComponent;

// Light management
struct GPULightData {
    Vector3 direction{0.0f, -1.0f, 0.0f};
    Vector3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    Vector3 ambient{0.3f, 0.3f, 0.3f};
};

class LightManager {
public:
    LightManager() = default;
    ~LightManager() = default;

    void RegisterLight(DirectionalLightComponent* light);
    void UnregisterLight(DirectionalLightComponent* light);
    void Clear();

    GPULightData BuildGPULightData() const;

    DirectionalLightComponent* GetDirectionalLight() const;

private:
    DirectionalLightComponent* directionalLight_ = nullptr;
};

} // namespace UnoEngine
