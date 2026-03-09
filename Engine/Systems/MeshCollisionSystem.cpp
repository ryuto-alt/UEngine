#include "pch.h"
#include "MeshCollisionSystem.h"
#include "../Core/Scene.h"
#include "../Core/GameObject.h"
#include "../Core/Transform.h"
#include "../Physics/CapsuleColliderComponent.h"
#include "../Physics/MeshColliderComponent.h"
#include "../Physics/RigidbodyComponent.h"
#include "../Math/Matrix.h"

#ifdef WITH_EDITOR
#include "../../Game/UI/EditorUI.h"
#endif

namespace UnoEngine {

void MeshCollisionSystem::OnSceneStart(Scene* /*scene*/) {}
void MeshCollisionSystem::OnSceneEnd(Scene* /*scene*/) {
    capsuleEntities_.clear();
    meshEntities_.clear();
}

void MeshCollisionSystem::OnUpdate(Scene* scene, float deltaTime) {
    if (!IsEnabled() || !scene) return;

#ifdef WITH_EDITOR
    auto* editorUI = scene->GetEditorUI();
    if (editorUI && !editorUI->IsPlaying()) return;
#endif

    GatherComponents(scene);

    for (auto& capsuleEnt : capsuleEntities_) {
        ProcessCapsule(capsuleEnt);
    }
}

void MeshCollisionSystem::GatherComponents(Scene* scene) {
    capsuleEntities_.clear();
    meshEntities_.clear();

    for (auto& obj : scene->GetGameObjects()) {
        if (!obj || !obj->IsActive()) continue;

        auto* capsule = obj->GetComponent<CapsuleColliderComponent>();
        if (capsule && capsule->IsEnabled()) {
            capsuleEntities_.push_back({obj.get(), capsule});
        }

        auto* meshCollider = obj->GetComponent<MeshColliderComponent>();
        if (meshCollider && meshCollider->IsEnabled() && meshCollider->IsBuilt()) {
            meshEntities_.push_back({obj.get(), meshCollider});
        }
    }
}

bool MeshCollisionSystem::QueryContacts(const Capsule& worldCapsule, const MeshEntity& meshEnt,
                                         std::vector<MeshContact>& outContacts) const {
    const auto* bvh = meshEnt.meshCollider->GetBVH();

    Matrix4x4 meshWorldMatrix = meshEnt.object->GetTransform().GetWorldMatrix();
    Matrix4x4 meshInverse = meshWorldMatrix.Inverse();

    // Transform capsule into mesh local space
    Capsule localCapsule;
    localCapsule.base = meshInverse.TransformPoint(worldCapsule.base);
    localCapsule.tip  = meshInverse.TransformPoint(worldCapsule.tip);

    // Conservative radius scaling
    float invScaleX = meshInverse.TransformDirection(Vector3::UnitX()).Length();
    float invScaleY = meshInverse.TransformDirection(Vector3::UnitY()).Length();
    float invScaleZ = meshInverse.TransformDirection(Vector3::UnitZ()).Length();
    float maxInvScale = std::max({invScaleX, invScaleY, invScaleZ});
    localCapsule.radius = worldCapsule.radius * maxInvScale;

    BoundingBox localCapsuleAABB = localCapsule.GetBoundingAABB();

    std::vector<uint32_t> candidateTriangles;
    bvh->QueryAABB(localCapsuleAABB, candidateTriangles);
    if (candidateTriangles.empty()) return false;

    bool anyHit = false;
    for (uint32_t triIdx : candidateTriangles) {
        const Triangle& tri = bvh->GetTriangle(triIdx);
        PenetrationResult result = CapsuleTriangleIntersect(localCapsule, tri);

        if (result.hit) {
            MeshContact contact;
            // Transform normal to world space
            contact.normal = meshWorldMatrix.TransformDirection(result.normal).Normalize();
            // Scale depth along the contact normal direction
            Vector3 localNormal = result.normal;
            Vector3 worldScaledNormal = meshWorldMatrix.TransformDirection(localNormal);
            contact.depth = result.depth * worldScaledNormal.Length();
            outContacts.push_back(contact);
            anyHit = true;
        }
    }
    return anyHit;
}

void MeshCollisionSystem::ProcessCapsule(CapsuleEntity& capsuleEnt) {
    auto& transform = capsuleEnt.object->GetTransform();
    auto* rb = capsuleEnt.object->GetComponent<RigidbodyComponent>();
    bool grounded = false;

    // 最終パスの接触法線を保持（速度スライド用）
    std::vector<Vector3> slideNormals;

    // Iterative depenetration — re-query each pass with updated capsule position
    for (uint32_t pass = 0; pass < kMaxDepenetrationPasses; ++pass) {
        Capsule worldCapsule = capsuleEnt.capsule->GetWorldCapsule();

        std::vector<MeshContact> contacts;
        for (auto& meshEnt : meshEntities_) {
            if (capsuleEnt.object == meshEnt.object) continue;
            QueryContacts(worldCapsule, meshEnt, contacts);
        }

        if (contacts.empty()) break;

        // 最深の地面contactと最深の壁contactだけを採用（加算しない）
        float bestGroundDepth = 0.0f;
        Vector3 bestGroundNormal = Vector3::Zero();
        float bestWallDepth = 0.0f;
        Vector3 bestWallNormal = Vector3::Zero();

        slideNormals.clear();

        for (const auto& contact : contacts) {
            if (contact.depth <= 0.0f) continue;

            slideNormals.push_back(contact.normal);

            if (contact.normal.GetY() > kGroundNormalThreshold) {
                if (contact.depth > bestGroundDepth) {
                    bestGroundDepth = contact.depth;
                    bestGroundNormal = contact.normal;
                }
                grounded = true;
            } else {
                if (contact.depth > bestWallDepth) {
                    bestWallDepth = contact.depth;
                    bestWallNormal = contact.normal;
                }
            }
        }

        // スキン幅を加算して振動を防止
        Vector3 totalPush = Vector3::Zero();
        if (bestGroundDepth > 0.0f) {
            totalPush = totalPush + bestGroundNormal * (bestGroundDepth + kSkinWidth);
        }
        if (bestWallDepth > 0.0f) {
            totalPush = totalPush + bestWallNormal * (bestWallDepth + kSkinWidth);
        }

        if (totalPush.LengthSq() < 1e-8f) break;

        Vector3 pos = transform.GetLocalPosition();
        transform.SetLocalPosition(pos + totalPush);
    }

    if (rb) {
        rb->SetGrounded(grounded);

        if (grounded) {
            Vector3 vel = rb->GetVelocity();
            if (vel.GetY() < 0.0f) {
                rb->SetVelocity(Vector3(vel.GetX(), 0.0f, vel.GetZ()));
            }
        }

        // 押し戻しパスで収集した法線で速度をスライド（再クエリ不要）
        Vector3 velocity = rb->GetVelocity();
        for (const auto& normal : slideNormals) {
            float vn = velocity.Dot(normal);
            if (vn < 0.0f) {
                velocity = velocity - normal * vn;
            }
        }
        rb->SetVelocity(velocity);
    }
}

} // namespace UnoEngine
