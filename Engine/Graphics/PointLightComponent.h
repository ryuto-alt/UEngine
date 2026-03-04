#pragma once

#include "../Core/Component.h"
#include "../Math/Vector.h"

namespace UnoEngine {

class PointLightComponent : public Component {
public:
    PointLightComponent() = default;

    void SetColor(const Vector3& color)     { color_ = color; }
    void SetIntensity(float intensity)      { intensity_ = intensity; }
    void SetRange(float range)              { range_ = range; }

    const Vector3& GetColor() const         { return color_; }
    float GetIntensity() const              { return intensity_; }
    float GetRange() const                  { return range_; }

private:
    Vector3 color_     = Vector3(1.0f, 1.0f, 1.0f);
    float intensity_   = 1.0f;
    float range_       = 10.0f;
};

} // namespace UnoEngine
