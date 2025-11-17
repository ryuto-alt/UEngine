#include "DirectionalLight.h"

namespace UnoEngine {

void DirectionalLight::SetDirection(const Vector3& direction) {
    direction_ = direction.Normalize();
}

void DirectionalLight::SetColor(const Vector3& color) {
    color_ = color;
}

void DirectionalLight::SetIntensity(float intensity) {
    intensity_ = intensity;
}

} // namespace UnoEngine
