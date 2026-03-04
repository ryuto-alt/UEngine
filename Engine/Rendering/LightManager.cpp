#include "pch.h"
#include "LightManager.h"
#include "../Graphics/DirectionalLightComponent.h"
#include "../Core/GameObject.h"
#include <algorithm>

namespace UnoEngine {

void LightManager::RegisterLight(DirectionalLightComponent* light) {
    directionalLight_ = light;
}

void LightManager::UnregisterLight(DirectionalLightComponent* light) {
    if (directionalLight_ == light) directionalLight_ = nullptr;
}

void LightManager::RegisterLight(PointLightComponent* light) {
    if (std::find(pointLights_.begin(), pointLights_.end(), light) == pointLights_.end())
        pointLights_.push_back(light);
}

void LightManager::UnregisterLight(PointLightComponent* light) {
    auto it = std::find(pointLights_.begin(), pointLights_.end(), light);
    if (it != pointLights_.end()) pointLights_.erase(it);
}

void LightManager::RegisterLight(SpotLightComponent* light) {
    if (std::find(spotLights_.begin(), spotLights_.end(), light) == spotLights_.end())
        spotLights_.push_back(light);
}

void LightManager::UnregisterLight(SpotLightComponent* light) {
    auto it = std::find(spotLights_.begin(), spotLights_.end(), light);
    if (it != spotLights_.end()) spotLights_.erase(it);
}

void LightManager::Clear() {
    directionalLight_ = nullptr;
    pointLights_.clear();
    spotLights_.clear();
}

DirectionalLightComponent* LightManager::GetDirectionalLight() const {
    return directionalLight_;
}

GPULightData LightManager::BuildGPULightData() const {
    GPULightData data;

    if (directionalLight_) {
        data.direction = directionalLight_->GetDirection();
        data.color     = directionalLight_->GetColor();
        data.intensity = directionalLight_->GetIntensity();
    }

    constexpr int kMaxPoint = 8;
    constexpr int kMaxSpot  = 4;
    static const float kDegToRad = 0.0174532925f;

    for (auto* p : pointLights_) {
        if (!p || !p->IsEnabled()) continue;
        if (static_cast<int>(data.pointLights.size()) >= kMaxPoint) break;
        auto* go = p->GetGameObject();
        GPULightData::PointLight pl;
        pl.position  = go ? go->GetTransform().GetLocalPosition() : Vector3{};
        pl.range     = p->GetRange();
        pl.color     = p->GetColor();
        pl.intensity = p->GetIntensity();
        data.pointLights.push_back(pl);
    }

    for (auto* s : spotLights_) {
        if (!s || !s->IsEnabled()) continue;
        if (static_cast<int>(data.spotLights.size()) >= kMaxSpot) break;
        auto* go = s->GetGameObject();
        GPULightData::SpotLight sl;
        sl.position   = go ? go->GetTransform().GetLocalPosition() : Vector3{};
        sl.range      = s->GetRange();
        sl.color      = s->GetColor();
        sl.intensity  = s->GetIntensity();
        sl.spotAngle  = s->GetSpotAngle() * kDegToRad;
        sl.innerAngle = s->GetInnerAngle() * kDegToRad;
        // Use -forward as spot direction
        sl.direction  = go ? -go->GetTransform().GetForward() : Vector3(0, -1, 0);
        data.spotLights.push_back(sl);
    }

    return data;
}

} // namespace UnoEngine
