#pragma once

#include "../Core/Component.h"
#include "DirectionalLight.h"

namespace UnoEngine {

class DirectionalLightComponent : public Component {
public:
    DirectionalLightComponent() = default;

    void OnUpdate(float deltaTime) override;

    void SetColor(const Vector3& color) { light_.SetColor(color); }
    void SetIntensity(float intensity) { light_.SetIntensity(intensity); }
    void SetDirection(const Vector3& direction) { 
        light_.SetDirection(direction);
        useTransform_ = false;
    }
    void UseTransformDirection(bool use) { useTransform_ = use; }

    Vector3 GetDirection() const { return light_.GetDirection(); }
    Vector3 GetColor() const { return light_.GetColor(); }
    float GetIntensity() const { return light_.GetIntensity(); }

    const DirectionalLight& GetLight() const { return light_; }

private:
    DirectionalLight light_;
    bool useTransform_ = true;
};

} // namespace UnoEngine
