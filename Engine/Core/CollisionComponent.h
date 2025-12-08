#pragma once

#include "Component.h"
#include "../Math/Vector.h"
#include <vector>

namespace UnoEngine {

// NavMeshエリアタイプ
enum class NavMeshAreaType : uint8_t {
    None = 0,       // NavMeshに含めない
    Walkable = 1,   // 歩行可能
    // 将来的に追加可能: Jump, Climb, Water など
};

struct AABB {
    Vector3 min;
    Vector3 max;

    [[nodiscard]] Vector3 GetCenter() const {
        return Vector3(
            (min.GetX() + max.GetX()) * 0.5f,
            (min.GetY() + max.GetY()) * 0.5f,
            (min.GetZ() + max.GetZ()) * 0.5f
        );
    }

    [[nodiscard]] Vector3 GetExtents() const {
        return Vector3(
            (max.GetX() - min.GetX()) * 0.5f,
            (max.GetY() - min.GetY()) * 0.5f,
            (max.GetZ() - min.GetZ()) * 0.5f
        );
    }

    [[nodiscard]] Vector3 GetSize() const {
        return Vector3(
            max.GetX() - min.GetX(),
            max.GetY() - min.GetY(),
            max.GetZ() - min.GetZ()
        );
    }
};

class CollisionComponent : public Component {
public:
    CollisionComponent() = default;
    ~CollisionComponent() override = default;

    void Awake() override;
    void Start() override;
    void OnUpdate(float deltaTime) override;

    // Enable/Disable collision
    [[nodiscard]] bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    // Collision state
    [[nodiscard]] bool IsColliding() const { return isColliding_; }
    void SetColliding(bool colliding) { isColliding_ = colliding; }

    // Single AABB (combined bounds)
    [[nodiscard]] const AABB& GetLocalAABB() const { return localAABB_; }
    [[nodiscard]] AABB GetWorldAABB() const;

    // Multiple AABBs for complex meshes
    [[nodiscard]] const std::vector<AABB>& GetLocalAABBs() const { return localAABBs_; }
    [[nodiscard]] std::vector<AABB> GetWorldAABBs() const;
    [[nodiscard]] bool HasMultipleAABBs() const { return !localAABBs_.empty(); }

    // Manual AABB override
    void SetLocalAABB(const AABB& aabb) { localAABB_ = aabb; localAABBs_.clear(); useAutoSize_ = false; }
    void SetLocalAABB(const Vector3& min, const Vector3& max);
    void SetLocalAABBs(std::vector<AABB> aabbs) { localAABBs_ = std::move(aabbs); useAutoSize_ = false; }

    // Auto-sizing from mesh bounds
    [[nodiscard]] bool IsAutoSized() const { return useAutoSize_; }
    void SetAutoSize(bool autoSize) { useAutoSize_ = autoSize; }
    void RecalculateFromMesh();

    // Collision layer/mask for filtering
    [[nodiscard]] uint32_t GetCollisionLayer() const { return collisionLayer_; }
    void SetCollisionLayer(uint32_t layer) { collisionLayer_ = layer; }

    [[nodiscard]] uint32_t GetCollisionMask() const { return collisionMask_; }
    void SetCollisionMask(uint32_t mask) { collisionMask_ = mask; }

    // Trigger vs solid collision
    [[nodiscard]] bool IsTrigger() const { return isTrigger_; }
    void SetTrigger(bool trigger) { isTrigger_ = trigger; }

    // Static vs dynamic (static objects don't get pushed)
    [[nodiscard]] bool IsStatic() const { return isStatic_; }
    void SetStatic(bool isStatic) { isStatic_ = isStatic; }

    // NavMesh area type
    [[nodiscard]] NavMeshAreaType GetNavMeshArea() const { return navMeshArea_; }
    void SetNavMeshArea(NavMeshAreaType area) { navMeshArea_ = area; }
    [[nodiscard]] bool IsNavMeshWalkable() const { return navMeshArea_ == NavMeshAreaType::Walkable; }

    // Static collision check
    [[nodiscard]] static bool CheckAABBCollision(const AABB& a, const AABB& b);
    [[nodiscard]] static Vector3 GetPenetrationVector(const AABB& a, const AABB& b);

private:
    AABB localAABB_{ Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f) };
    std::vector<AABB> localAABBs_;
    bool enabled_ = true;
    bool isColliding_ = false;
    bool useAutoSize_ = true;
    bool isTrigger_ = false;
    bool isStatic_ = false;
    NavMeshAreaType navMeshArea_ = NavMeshAreaType::None;  // デフォルトはNavMeshに含めない
    uint32_t collisionLayer_ = 0;  // Default to layer 0
    uint32_t collisionMask_ = 0xFFFFFFFF;
};

} // namespace UnoEngine
