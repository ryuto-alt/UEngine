#pragma once

#include "../Math/Vector.h"

namespace UnoEngine {

class DirectionalLight {
public:
    DirectionalLight() = default;

    void SetDirection(const Vector3& direction);
    void SetColor(const Vector3& color);
    void SetIntensity(float intensity);

    const Vector3& GetDirection() const { return direction_; }
    const Vector3& GetColor() const { return color_; }
    float GetIntensity() const { return intensity_; }

private:
    Vector3 direction_ = Vector3(0.0f, -1.0f, 0.0f);  // デフォルトは下向き
    Vector3 color_ = Vector3(1.0f, 1.0f, 1.0f);       // 白色光
    float intensity_ = 1.0f;
};

} // namespace UnoEngine
