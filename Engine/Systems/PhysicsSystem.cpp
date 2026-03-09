#include "pch.h"
#include "PhysicsSystem.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"
#include "../Core/CollisionComponent.h"
#include "../Physics/RigidbodyComponent.h"
#include "../Core/Transform.h"

#ifdef WITH_EDITOR
#include "../../Game/UI/EditorUI.h"
#endif

namespace UnoEngine {

void PhysicsSystem::OnSceneStart(Scene* /*scene*/) {}
void PhysicsSystem::OnSceneEnd(Scene* /*scene*/) {}

void PhysicsSystem::OnUpdate(Scene* scene, float deltaTime) {
    if (!IsEnabled() || !scene) return;

#ifdef WITH_EDITOR
    auto* editorUI = scene->GetEditorUI();
    if (editorUI && !editorUI->IsPlaying()) return;
#endif

    for (auto& obj : scene->GetGameObjects()) {
        if (!obj || !obj->IsActive()) continue;

        auto* rb = obj->GetComponent<RigidbodyComponent>();
        if (!rb || !rb->IsEnabled() || rb->IsKinematic()) continue;

        auto* collision = obj->GetComponent<CollisionComponent>();
        auto& transform = obj->GetTransform();
        auto pos        = transform.GetLocalPosition();

        // MeshCollisionSystem (or previous frame) may have set grounded
        bool wasGrounded = rb->isGrounded_;
        bool aabbColliding = collision && collision->IsColliding();

        // Gravity — skip if grounded (from any collision source)
        if (rb->useGravity_ && !wasGrounded && !aabbColliding) {
            float vy = rb->velocity_.GetY() + kGravity * deltaTime;
            rb->velocity_ = Vector3(rb->velocity_.GetX(), vy, rb->velocity_.GetZ());
        }

        // Clamp downward velocity when grounded
        if ((wasGrounded || aabbColliding) && rb->velocity_.GetY() < 0.0f) {
            rb->velocity_ = Vector3(rb->velocity_.GetX(), 0.0f, rb->velocity_.GetZ());
        }

        // Reset grounded — will be re-set by MeshCollisionSystem / CollisionSystem
        rb->isGrounded_ = false;

        // Integrate position
        float nx = pos.GetX() + rb->velocity_.GetX() * deltaTime;
        float ny = pos.GetY() + rb->velocity_.GetY() * deltaTime;
        float nz = pos.GetZ() + rb->velocity_.GetZ() * deltaTime;
        transform.SetLocalPosition(Vector3(nx, ny, nz));

        // Drag (exponential decay)
        float damping = 1.0f - rb->drag_ * deltaTime;
        if (damping < 0.0f) damping = 0.0f;
        rb->velocity_ = Vector3(
            rb->velocity_.GetX() * damping,
            rb->velocity_.GetY(),           // gravity handles vertical
            rb->velocity_.GetZ() * damping
        );
    }
}

} // namespace UnoEngine
