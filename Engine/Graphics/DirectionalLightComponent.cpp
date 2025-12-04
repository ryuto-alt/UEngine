#include "pch.h"
#include "DirectionalLightComponent.h"
#include "../Core/GameObject.h"

namespace UnoEngine {

void DirectionalLightComponent::OnUpdate(float deltaTime) {
    if (useTransform_ && GetGameObject()) {
        Vector3 forward = GetGameObject()->GetTransform().GetForward();
        light_.SetDirection(forward);
    }
}

} // namespace UnoEngine
