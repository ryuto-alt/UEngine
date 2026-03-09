#include "pch.h"
#include "BVHTree.h"
#include "../Graphics/Mesh.h"
#include <algorithm>
#include <numeric>

namespace UnoEngine {

void BVHTree::Build(std::span<const Vector3> positions, std::span<const uint32_t> indices) {
    if (indices.size() < 3) return;

    uint32_t triCount = static_cast<uint32_t>(indices.size() / 3);
    triangles_.resize(triCount);

    for (uint32_t i = 0; i < triCount; ++i) {
        triangles_[i].v0 = positions[indices[i * 3 + 0]];
        triangles_[i].v1 = positions[indices[i * 3 + 1]];
        triangles_[i].v2 = positions[indices[i * 3 + 2]];
    }

    triangleOrder_.resize(triCount);
    std::iota(triangleOrder_.begin(), triangleOrder_.end(), 0);

    nodes_.clear();
    nodes_.reserve(triCount * 2);

    BuildRecursive(0, triCount);

    // Reorder triangles by build order for cache coherence
    std::vector<Triangle> reordered(triCount);
    for (uint32_t i = 0; i < triCount; ++i) {
        reordered[i] = triangles_[triangleOrder_[i]];
    }
    triangles_ = std::move(reordered);
}

void BVHTree::BuildFromMeshData(std::span<const Vertex> vertices, std::span<const uint32_t> indices) {
    if (indices.size() < 3) return;

    uint32_t triCount = static_cast<uint32_t>(indices.size() / 3);
    triangles_.resize(triCount);

    for (uint32_t i = 0; i < triCount; ++i) {
        const auto& v0 = vertices[indices[i * 3 + 0]];
        const auto& v1 = vertices[indices[i * 3 + 1]];
        const auto& v2 = vertices[indices[i * 3 + 2]];
        triangles_[i].v0 = Vector3(v0.px, v0.py, v0.pz);
        triangles_[i].v1 = Vector3(v1.px, v1.py, v1.pz);
        triangles_[i].v2 = Vector3(v2.px, v2.py, v2.pz);
    }

    triangleOrder_.resize(triCount);
    std::iota(triangleOrder_.begin(), triangleOrder_.end(), 0);

    nodes_.clear();
    nodes_.reserve(triCount * 2);

    BuildRecursive(0, triCount);

    std::vector<Triangle> reordered(triCount);
    for (uint32_t i = 0; i < triCount; ++i) {
        reordered[i] = triangles_[triangleOrder_[i]];
    }
    triangles_ = std::move(reordered);
}

uint32_t BVHTree::BuildRecursive(uint32_t begin, uint32_t end) {
    uint32_t nodeIdx = static_cast<uint32_t>(nodes_.size());
    nodes_.emplace_back();

    // Compute bounds for all triangles in range
    for (uint32_t i = begin; i < end; ++i) {
        BoundingBox triBounds = triangles_[triangleOrder_[i]].GetBoundingBox();
        nodes_[nodeIdx].bounds.Expand(triBounds);
    }

    uint32_t count = end - begin;
    if (count <= kMaxLeafTriangles) {
        nodes_[nodeIdx].triangleOffset = begin;
        nodes_[nodeIdx].triangleCount = count;
        return nodeIdx;
    }

    // SAH: find best split axis and bucket
    float bestCost = std::numeric_limits<float>::max();
    uint32_t bestAxis = 0;
    uint32_t bestBucket = 1;

    Vector3 boundsSize = nodes_[nodeIdx].bounds.GetSize();
    float parentArea = 2.0f * (boundsSize.GetX() * boundsSize.GetY() +
                               boundsSize.GetY() * boundsSize.GetZ() +
                               boundsSize.GetZ() * boundsSize.GetX());

    if (parentArea < 1e-10f) {
        nodes_[nodeIdx].triangleOffset = begin;
        nodes_[nodeIdx].triangleCount = count;
        return nodeIdx;
    }

    for (uint32_t axis = 0; axis < 3; ++axis) {
        // Sort triangles by centroid along axis
        std::sort(triangleOrder_.begin() + begin, triangleOrder_.begin() + end,
            [this, axis](uint32_t a, uint32_t b) {
                Vector3 ca = triangles_[a].GetCentroid();
                Vector3 cb = triangles_[b].GetCentroid();
                float va = (axis == 0) ? ca.GetX() : (axis == 1) ? ca.GetY() : ca.GetZ();
                float vb = (axis == 0) ? cb.GetX() : (axis == 1) ? cb.GetY() : cb.GetZ();
                return va < vb;
            });

        // Bucket-based SAH evaluation
        struct Bucket {
            BoundingBox bounds;
            uint32_t count = 0;
        };
        std::array<Bucket, kSAHBuckets> buckets{};

        // Compute centroid bounds for binning
        float cMin = std::numeric_limits<float>::max();
        float cMax = std::numeric_limits<float>::lowest();
        for (uint32_t i = begin; i < end; ++i) {
            Vector3 c = triangles_[triangleOrder_[i]].GetCentroid();
            float cv = (axis == 0) ? c.GetX() : (axis == 1) ? c.GetY() : c.GetZ();
            cMin = std::min(cMin, cv);
            cMax = std::max(cMax, cv);
        }

        if (cMax - cMin < 1e-6f) continue;

        float scale = static_cast<float>(kSAHBuckets) / (cMax - cMin);
        for (uint32_t i = begin; i < end; ++i) {
            Vector3 c = triangles_[triangleOrder_[i]].GetCentroid();
            float cv = (axis == 0) ? c.GetX() : (axis == 1) ? c.GetY() : c.GetZ();
            uint32_t b = std::min(static_cast<uint32_t>((cv - cMin) * scale),
                                  kSAHBuckets - 1);
            buckets[b].bounds.Expand(triangles_[triangleOrder_[i]].GetBoundingBox());
            buckets[b].count++;
        }

        // Evaluate splits
        for (uint32_t split = 1; split < kSAHBuckets; ++split) {
            BoundingBox leftBox, rightBox;
            uint32_t leftCount = 0, rightCount = 0;

            for (uint32_t j = 0; j < split; ++j) {
                leftBox.Expand(buckets[j].bounds);
                leftCount += buckets[j].count;
            }
            for (uint32_t j = split; j < kSAHBuckets; ++j) {
                rightBox.Expand(buckets[j].bounds);
                rightCount += buckets[j].count;
            }

            if (leftCount == 0 || rightCount == 0) continue;

            Vector3 ls = leftBox.GetSize();
            Vector3 rs = rightBox.GetSize();
            float leftArea  = 2.0f * (ls.GetX()*ls.GetY() + ls.GetY()*ls.GetZ() + ls.GetZ()*ls.GetX());
            float rightArea = 2.0f * (rs.GetX()*rs.GetY() + rs.GetY()*rs.GetZ() + rs.GetZ()*rs.GetX());

            float cost = kTraversalCost + kIntersectCost *
                         (leftArea * leftCount + rightArea * rightCount) / parentArea;

            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestBucket = split;
            }
        }
    }

    // Leaf cost: all triangles tested directly
    float leafCost = kIntersectCost * count;
    if (bestCost >= leafCost) {
        nodes_[nodeIdx].triangleOffset = begin;
        nodes_[nodeIdx].triangleCount = count;
        return nodeIdx;
    }

    // Sort by best axis for the final split
    std::sort(triangleOrder_.begin() + begin, triangleOrder_.begin() + end,
        [this, bestAxis](uint32_t a, uint32_t b) {
            Vector3 ca = triangles_[a].GetCentroid();
            Vector3 cb = triangles_[b].GetCentroid();
            float va = (bestAxis == 0) ? ca.GetX() : (bestAxis == 1) ? ca.GetY() : ca.GetZ();
            float vb = (bestAxis == 0) ? cb.GetX() : (bestAxis == 1) ? cb.GetY() : cb.GetZ();
            return va < vb;
        });

    // Compute bestSplit from sorted order using the best bucket boundary
    float cMin = std::numeric_limits<float>::max();
    float cMax = std::numeric_limits<float>::lowest();
    for (uint32_t i = begin; i < end; ++i) {
        Vector3 c = triangles_[triangleOrder_[i]].GetCentroid();
        float cv = (bestAxis == 0) ? c.GetX() : (bestAxis == 1) ? c.GetY() : c.GetZ();
        cMin = std::min(cMin, cv);
        cMax = std::max(cMax, cv);
    }
    float splitPos = cMin + (cMax - cMin) * static_cast<float>(bestBucket) / static_cast<float>(kSAHBuckets);
    uint32_t bestSplit = begin;
    for (uint32_t i = begin; i < end; ++i) {
        Vector3 c = triangles_[triangleOrder_[i]].GetCentroid();
        float cv = (bestAxis == 0) ? c.GetX() : (bestAxis == 1) ? c.GetY() : c.GetZ();
        if (cv < splitPos) bestSplit = i + 1;
    }
    bestSplit = std::clamp(bestSplit, begin + 1, end - 1);

    // Must use nodeIdx (not node reference) — recursive calls may reallocate nodes_
    nodes_[nodeIdx].leftChild  = BuildRecursive(begin, bestSplit);
    nodes_[nodeIdx].rightChild = BuildRecursive(bestSplit, end);
    return nodeIdx;
}

void BVHTree::QueryAABB(const BoundingBox& queryBounds, std::vector<uint32_t>& outTriangleIndices) const {
    if (nodes_.empty()) return;
    QueryAABBRecursive(0, queryBounds, outTriangleIndices);
}

void BVHTree::QueryAABBRecursive(uint32_t nodeIdx, const BoundingBox& queryBounds,
                                  std::vector<uint32_t>& outTriangleIndices) const {
    const auto& node = nodes_[nodeIdx];
    if (!node.bounds.Intersects(queryBounds)) return;

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.triangleCount; ++i) {
            outTriangleIndices.push_back(node.triangleOffset + i);
        }
        return;
    }

    QueryAABBRecursive(node.leftChild, queryBounds, outTriangleIndices);
    QueryAABBRecursive(node.rightChild, queryBounds, outTriangleIndices);
}

std::optional<BVHRayHit> BVHTree::Raycast(const Vector3& origin, const Vector3& dir, float maxDist) const {
    if (nodes_.empty()) return std::nullopt;

    Vector3 normDir = dir.Normalize();
    BVHRayHit closest;
    closest.distance = maxDist;

    RaycastRecursive(0, origin, normDir, closest);

    if (closest.distance < maxDist) {
        return closest;
    }
    return std::nullopt;
}

void BVHTree::RaycastRecursive(uint32_t nodeIdx, const Vector3& origin, const Vector3& dir,
                                BVHRayHit& closest) const {
    const auto& node = nodes_[nodeIdx];

    // IntersectsRay takes direction (not inverse), handles near-zero internally
    float tMin, tMax;
    if (!node.bounds.IntersectsRay(origin, dir, tMin, tMax)) {
        return;
    }
    if (tMin > closest.distance) return;

    if (node.IsLeaf()) {
        for (uint32_t i = 0; i < node.triangleCount; ++i) {
            uint32_t triIdx = node.triangleOffset + i;
            auto result = RayTriangleIntersect(origin, dir, triangles_[triIdx]);
            if (result && *result < closest.distance) {
                closest.distance = *result;
                closest.point = origin + dir * (*result);
                closest.normal = triangles_[triIdx].GetNormal();
                closest.triangleIndex = triIdx;
            }
        }
        return;
    }

    RaycastRecursive(node.leftChild, origin, dir, closest);
    RaycastRecursive(node.rightChild, origin, dir, closest);
}

} // namespace UnoEngine
