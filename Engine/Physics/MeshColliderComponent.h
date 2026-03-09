#pragma once

#include "../Core/Component.h"
#include "BVHTree.h"
#include <memory>

namespace UnoEngine {

class MeshColliderComponent : public Component {
public:
    MeshColliderComponent() = default;
    ~MeshColliderComponent() override = default;

    void Awake() override;

    void RebuildBVH();

    bool IsBuilt() const { return bvh_ && bvh_->IsBuilt(); }
    const BVHTree* GetBVH() const { return bvh_.get(); }
    uint32_t GetTriangleCount() const { return bvh_ ? bvh_->GetTriangleCount() : 0; }

private:
    std::unique_ptr<BVHTree> bvh_;
};

} // namespace UnoEngine
