#pragma once

#include "Vector.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace UnoEngine {

struct BoundingBox {
    Vector3 min;
    Vector3 max;

    BoundingBox() 
        : min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max())
        , max(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()) {}

    BoundingBox(const Vector3& minPoint, const Vector3& maxPoint)
        : min(minPoint), max(maxPoint) {}

    Vector3 GetCenter() const { 
        return (min + max) * 0.5f; 
    }

    Vector3 GetExtents() const { 
        return (max - min) * 0.5f; 
    }

    Vector3 GetSize() const {
        return max - min;
    }

    bool IsValid() const {
        return min.GetX() <= max.GetX() && 
               min.GetY() <= max.GetY() && 
               min.GetZ() <= max.GetZ();
    }

    bool Contains(const Vector3& point) const {
        return point.GetX() >= min.GetX() && point.GetX() <= max.GetX() &&
               point.GetY() >= min.GetY() && point.GetY() <= max.GetY() &&
               point.GetZ() >= min.GetZ() && point.GetZ() <= max.GetZ();
    }

    bool Intersects(const BoundingBox& other) const {
        return min.GetX() <= other.max.GetX() && max.GetX() >= other.min.GetX() &&
               min.GetY() <= other.max.GetY() && max.GetY() >= other.min.GetY() &&
               min.GetZ() <= other.max.GetZ() && max.GetZ() >= other.min.GetZ();
    }

    void Expand(const Vector3& point) {
        min = Vector3(
            std::min(min.GetX(), point.GetX()),
            std::min(min.GetY(), point.GetY()),
            std::min(min.GetZ(), point.GetZ())
        );
        max = Vector3(
            std::max(max.GetX(), point.GetX()),
            std::max(max.GetY(), point.GetY()),
            std::max(max.GetZ(), point.GetZ())
        );
    }

    void Expand(const BoundingBox& other) {
        if (!other.IsValid()) return;
        Expand(other.min);
        Expand(other.max);
    }

    static BoundingBox CreateFromPoints(const Vector3* points, size_t count) {
        BoundingBox box;
        for (size_t i = 0; i < count; ++i) {
            box.Expand(points[i]);
        }
        return box;
    }
};

struct BoundingSphere {
    Vector3 center;
    float radius = 0.0f;

    BoundingSphere() = default;

    BoundingSphere(const Vector3& c, float r)
        : center(c), radius(r) {}

    bool Contains(const Vector3& point) const {
        return (point - center).LengthSq() <= radius * radius;
    }

    bool Intersects(const BoundingSphere& other) const {
        float distSq = (other.center - center).LengthSq();
        float radiusSum = radius + other.radius;
        return distSq <= radiusSum * radiusSum;
    }

    bool Intersects(const BoundingBox& box) const {
        // Find closest point on box to sphere center
        Vector3 closest(
            std::clamp(center.GetX(), box.min.GetX(), box.max.GetX()),
            std::clamp(center.GetY(), box.min.GetY(), box.max.GetY()),
            std::clamp(center.GetZ(), box.min.GetZ(), box.max.GetZ())
        );
        return (closest - center).LengthSq() <= radius * radius;
    }

    static BoundingSphere CreateFromBox(const BoundingBox& box) {
        BoundingSphere sphere;
        sphere.center = box.GetCenter();
        sphere.radius = box.GetExtents().Length();
        return sphere;
    }

    static BoundingSphere CreateFromPoints(const Vector3* points, size_t count) {
        if (count == 0) return BoundingSphere();

        // Simple bounding sphere: use bounding box center and max distance
        BoundingBox box = BoundingBox::CreateFromPoints(points, count);
        BoundingSphere sphere;
        sphere.center = box.GetCenter();
        
        float maxDistSq = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            float distSq = (points[i] - sphere.center).LengthSq();
            maxDistSq = std::max(maxDistSq, distSq);
        }
        sphere.radius = std::sqrt(maxDistSq);
        
        return sphere;
    }
};

} // namespace UnoEngine
