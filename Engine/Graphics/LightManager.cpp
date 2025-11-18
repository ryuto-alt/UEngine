#include "LightManager.h"
#include "DirectionalLightComponent.h"

namespace UnoEngine {

void LightManager::RegisterLight(DirectionalLightComponent* light) {
    directionalLight_ = light;
}

void LightManager::UnregisterLight(DirectionalLightComponent* light) {
    if (directionalLight_ == light) {
        directionalLight_ = nullptr;
    }
}

void LightManager::Clear() {
    directionalLight_ = nullptr;
}

DirectionalLightComponent* LightManager::GetDirectionalLight() const {
    return directionalLight_;
}

} // namespace UnoEngine
