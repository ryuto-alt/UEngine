#include "pch.h"
#include "LightManager.h"
#include "../Graphics/DirectionalLightComponent.h"

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

GPULightData LightManager::BuildGPULightData() const {
    GPULightData data;
    if (directionalLight_) {
        data.direction = directionalLight_->GetDirection();
        data.color = directionalLight_->GetColor();
        data.intensity = directionalLight_->GetIntensity();
    }
    return data;
}

} // namespace UnoEngine
