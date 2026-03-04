#pragma once

#include "../Core/Component.h"
#include "../Math/Vector.h"

namespace UnoEngine {

class RigidbodyComponent : public Component {
public:
    RigidbodyComponent() = default;
    ~RigidbodyComponent() override = default;

    void OnUpdate(float deltaTime) override {}

    void AddForce(const Vector3& force) {
        velocity_ = Vector3(
            velocity_.GetX() + force.GetX() / mass_,
            velocity_.GetY() + force.GetY() / mass_,
            velocity_.GetZ() + force.GetZ() / mass_
        );
    }

    void AddImpulse(const Vector3& impulse) {
        velocity_ = Vector3(
            velocity_.GetX() + impulse.GetX(),
            velocity_.GetY() + impulse.GetY(),
            velocity_.GetZ() + impulse.GetZ()
        );
    }

    const Vector3& GetVelocity() const { return velocity_; }
    void SetVelocity(const Vector3& v) { velocity_ = v; }

    float GetMass() const { return mass_; }
    void SetMass(float m) { mass_ = m; }

    float GetDrag() const { return drag_; }
    void SetDrag(float d) { drag_ = d; }

    bool UseGravity() const { return useGravity_; }
    void SetUseGravity(bool g) { useGravity_ = g; }

    bool IsKinematic() const { return isKinematic_; }
    void SetKinematic(bool k) { isKinematic_ = k; }

    bool IsGrounded() const { return isGrounded_; }
    void SetGrounded(bool g) { isGrounded_ = g; }

private:
    friend class PhysicsSystem;

    Vector3 velocity_{};
    float   mass_        = 1.0f;
    float   drag_        = 0.05f;
    bool    useGravity_  = true;
    bool    isKinematic_ = false;
    bool    isGrounded_  = false;
};

} // namespace UnoEngine
