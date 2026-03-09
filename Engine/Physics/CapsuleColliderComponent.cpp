#include "pch.h"
#include "CapsuleColliderComponent.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"

namespace UnoEngine {

Capsule CapsuleColliderComponent::GetWorldCapsule() const {
    Capsule capsule;
    capsule.radius = radius_;

    auto& transform = GetGameObject()->GetTransform();
    Matrix4x4 worldMatrix = transform.GetWorldMatrix();

    capsule.base = worldMatrix.TransformPoint(localBase_);
    capsule.tip  = worldMatrix.TransformPoint(localTip_);

    // Scale radius by max XZ scale (uniform scale assumed for capsule)
    Vector3 scaleX = worldMatrix.TransformDirection(Vector3::UnitX());
    Vector3 scaleZ = worldMatrix.TransformDirection(Vector3::UnitZ());
    float maxScale = std::max(scaleX.Length(), scaleZ.Length());
    capsule.radius *= maxScale;

    return capsule;
}

} // namespace UnoEngine
