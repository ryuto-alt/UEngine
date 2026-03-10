#pragma once

#include "../Core/Component.h"
#include "../Math/GeometryUtils.h"
#include "../Math/Matrix.h"

namespace UnoEngine {

class CapsuleColliderComponent : public Component {
public:
    CapsuleColliderComponent() = default;
    ~CapsuleColliderComponent() override = default;

    // Local-space capsule definition
    void SetLocalBase(const Vector3& base) { localBase_ = base; }
    void SetLocalTip(const Vector3& tip) { localTip_ = tip; }
    void SetRadius(float radius) { radius_ = radius; }

    const Vector3& GetLocalBase() const { return localBase_; }
    const Vector3& GetLocalTip() const { return localTip_; }
    float GetRadius() const { return radius_; }

    // 階段を登れる最大段差の高さ
    void SetMaxStepHeight(float height) { maxStepHeight_ = height; }
    float GetMaxStepHeight() const { return maxStepHeight_; }

    // Height = distance between base and tip (not including radius caps)
    void SetFromHeightRadius(float height, float radius) {
        radius_ = radius;
        localBase_ = Vector3(0.0f, radius, 0.0f);
        localTip_ = Vector3(0.0f, height - radius, 0.0f);
    }

    // Compute world-space capsule using owner's transform
    Capsule GetWorldCapsule() const;

private:
    Vector3 localBase_ = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 localTip_  = Vector3(0.0f, 1.0f, 0.0f);
    float radius_ = 0.3f;
    float maxStepHeight_ = 0.4f;  // 階段を登れる最大段差高さ
};

} // namespace UnoEngine
