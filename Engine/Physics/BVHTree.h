#pragma once

#include "../Math/BoundingVolume.h"
#include "../Math/GeometryUtils.h"
#include <vector>
#include <optional>
#include <cstdint>
#include <span>

namespace UnoEngine {

struct Vertex;

struct BVHNode {
    BoundingBox bounds;
    uint32_t leftChild  = 0;
    uint32_t rightChild = 0;
    uint32_t triangleOffset = 0;
    uint32_t triangleCount  = 0;

    bool IsLeaf() const { return triangleCount > 0; }
};

struct BVHRayHit {
    float distance = 0.0f;
    Vector3 point;
    Vector3 normal;
    uint32_t triangleIndex = 0;
};

class BVHTree {
public:
    BVHTree() = default;

    // Build from raw vertex/index data
    void Build(std::span<const Vector3> positions, std::span<const uint32_t> indices);

    // Build from Mesh vertex/index data
    void BuildFromMeshData(std::span<const Vertex> vertices, std::span<const uint32_t> indices);

    // Query all triangles whose AABB overlaps the given bounds
    void QueryAABB(const BoundingBox& queryBounds, std::vector<uint32_t>& outTriangleIndices) const;

    // Raycast against the BVH
    std::optional<BVHRayHit> Raycast(const Vector3& origin, const Vector3& dir, float maxDist) const;

    bool IsBuilt() const { return !nodes_.empty(); }
    uint32_t GetTriangleCount() const { return static_cast<uint32_t>(triangles_.size()); }
    const Triangle& GetTriangle(uint32_t index) const { return triangles_[index]; }
    const std::vector<BVHNode>& GetNodes() const { return nodes_; }

private:
    static constexpr uint32_t kMaxLeafTriangles = 4;
    static constexpr uint32_t kSAHBuckets = 12;
    static constexpr float kTraversalCost = 1.0f;
    static constexpr float kIntersectCost = 1.5f;

    uint32_t BuildRecursive(uint32_t begin, uint32_t end);
    void QueryAABBRecursive(uint32_t nodeIdx, const BoundingBox& queryBounds,
                            std::vector<uint32_t>& outTriangleIndices) const;
    void RaycastRecursive(uint32_t nodeIdx, const Vector3& origin, const Vector3& dir,
                          BVHRayHit& closest) const;

    std::vector<BVHNode> nodes_;
    std::vector<Triangle> triangles_;
    std::vector<uint32_t> triangleOrder_;  // reordered indices during build
};

} // namespace UnoEngine
