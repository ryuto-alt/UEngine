#pragma once

#include "DirectionalLight.h"

namespace UnoEngine {

class DirectionalLightComponent;

// Light management
class LightManager {
public:
    LightManager() = default;
    ~LightManager() = default;

    void RegisterLight(DirectionalLightComponent* light);
    void UnregisterLight(DirectionalLightComponent* light);
    void Clear();

    DirectionalLightComponent* GetDirectionalLight() const;

private:
    DirectionalLightComponent* directionalLight_ = nullptr;
};

} // namespace UnoEngine
