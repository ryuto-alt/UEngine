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

void LightManager::SyncFromScene(const std::vector<UniquePtr<GameObject>>& objects) {
    // clear() keeps capacity, avoiding reallocation each frame
    directionalLight_ = nullptr;
    pointLights_.clear();
    spotLights_.clear();
    for (const auto& obj : objects) {
        if (!obj) continue;
        if (auto* dl = obj->GetComponent<DirectionalLightComponent>()) {
            RegisterLight(dl);
        }
        if (auto* pl = obj->GetComponent<PointLightComponent>()) {
            RegisterLight(pl);
        }
        if (auto* sl = obj->GetComponent<SpotLightComponent>()) {
            RegisterLight(sl);
        }
    }
}

GPULightData LightManager::BuildGPULightData() const {
    GPULightData data;

    if (directionalLight_) {
        data.direction = directionalLight_->GetDirection();
        data.color     = directionalLight_->GetColor();
        data.intensity = directionalLight_->GetIntensity();
    } else {
        // No directional light in scene: zero out so it doesn't illuminate
        data.intensity = 0.0f;
    }

    static const float kDegToRad = 0.0174532925f;

    uint32 pointIdx = 0;
    for (auto* p : pointLights_) {
        if (!p || !p->IsEnabled()) continue;
        if (pointIdx >= GPULightData::kMaxPointLights) break;
        auto* go = p->GetGameObject();
        auto& pl = data.pointLights[pointIdx];
        pl.position  = go ? go->GetTransform().GetLocalPosition() : Vector3{};
        pl.range     = p->GetRange();
        pl.color     = p->GetColor();
        pl.intensity = p->GetIntensity();
        ++pointIdx;
    }
    data.pointLightCount = pointIdx;

    uint32 spotIdx = 0;
    for (auto* s : spotLights_) {
        if (!s || !s->IsEnabled()) continue;
        if (spotIdx >= GPULightData::kMaxSpotLights) break;
        auto* go = s->GetGameObject();
        auto& sl = data.spotLights[spotIdx];
        sl.position   = go ? go->GetTransform().GetLocalPosition() : Vector3{};
        sl.range      = s->GetRange();
        sl.color      = s->GetColor();
        sl.intensity  = s->GetIntensity();
        sl.spotAngle  = s->GetSpotAngle() * kDegToRad;
        sl.innerAngle = s->GetInnerAngle() * kDegToRad;
        sl.direction  = go ? -go->GetTransform().GetForward() : Vector3(0, -1, 0);
        ++spotIdx;
    }
    data.spotLightCount = spotIdx;

    return data;
}

} // namespace UnoEngine
