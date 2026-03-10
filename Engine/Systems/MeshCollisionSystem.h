#pragma once

#include "ISystem.h"
#include "../Math/GeometryUtils.h"
#include <vector>

namespace UnoEngine {

class GameObject;
class CapsuleColliderComponent;
class MeshColliderComponent;

struct MeshContact {
    Vector3 normal;      // 接触点からの分離方向（エッジでは水平になりうる）
    Vector3 faceNormal;  // 三角形の表面法線（地面判定に使用）
    float depth = 0.0f;
};

class MeshCollisionSystem : public ISystem {
public:
    MeshCollisionSystem() = default;
    ~MeshCollisionSystem() override = default;

    void OnSceneStart(Scene* scene) override;
    void OnUpdate(Scene* scene, float deltaTime) override;
    void OnSceneEnd(Scene* scene) override;

    int GetPriority() const override { return 45; }

    void SetDebugDraw(bool enabled) { debugDraw_ = enabled; }
    bool IsDebugDrawEnabled() const { return debugDraw_; }

private:
    static constexpr float kGroundNormalThreshold = 0.7f;
    static constexpr uint32_t kMaxDepenetrationPasses = 4;
    static constexpr float kSkinWidth = 0.005f;
    static constexpr float kDefaultMaxStepHeight = 0.4f;  // デフォルトの最大ステップ高

    struct CapsuleEntity {
        GameObject* object = nullptr;
        CapsuleColliderComponent* capsule = nullptr;
    };

    struct MeshEntity {
        GameObject* object = nullptr;
        MeshColliderComponent* meshCollider = nullptr;
    };

    void GatherComponents(Scene* scene);
    void ProcessCapsule(CapsuleEntity& capsuleEnt);
    bool QueryContacts(const Capsule& worldCapsule, const MeshEntity& meshEnt,
                       std::vector<MeshContact>& outContacts) const;

    std::vector<CapsuleEntity> capsuleEntities_;
    std::vector<MeshEntity> meshEntities_;
    bool debugDraw_ = false;
};

} // namespace UnoEngine
