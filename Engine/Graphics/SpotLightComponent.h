#pragma once

#include "../Core/Component.h"
#include "../Math/Vector.h"

namespace UnoEngine {

class SpotLightComponent : public Component {
public:
    SpotLightComponent() = default;

    void SetColor(const Vector3& color)       { color_ = color; }
    void SetIntensity(float intensity)        { intensity_ = intensity; }
    void SetRange(float range)                { range_ = range; }
    void SetSpotAngle(float angleDeg)         { spotAngle_ = angleDeg; }
    void SetInnerAngle(float angleDeg)        { innerAngle_ = angleDeg; }

    const Vector3& GetColor() const           { return color_; }
    float GetIntensity() const                { return intensity_; }
    float GetRange() const                    { return range_; }
    float GetSpotAngle() const                { return spotAngle_; }
    float GetInnerAngle() const               { return innerAngle_; }

private:
    Vector3 color_     = Vector3(1.0f, 1.0f, 1.0f);
    float intensity_   = 1.0f;
    float range_       = 10.0f;
    float spotAngle_   = 30.0f;   // outer cone half-angle (degrees)
    float innerAngle_  = 15.0f;   // inner cone half-angle (degrees)
};

} // namespace UnoEngine
