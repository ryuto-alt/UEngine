#pragma once

#include "Vector.h"
#include "BoundingVolume.h"
#include <optional>
#include <cmath>
#include <algorithm>

namespace UnoEngine {

struct Triangle {
    Vector3 v0, v1, v2;

    Vector3 GetNormal() const {
        return (v1 - v0).Cross(v2 - v0).Normalize();
    }

    Vector3 GetCentroid() const {
        return (v0 + v1 + v2) * (1.0f / 3.0f);
    }

    BoundingBox GetBoundingBox() const {
        BoundingBox box;
        box.Expand(v0);
        box.Expand(v1);
        box.Expand(v2);
        return box;
    }
};

struct Capsule {
    Vector3 base;
    Vector3 tip;
    float radius = 0.3f;

    Vector3 GetCenter() const { return (base + tip) * 0.5f; }
    float GetHeight() const { return (tip - base).Length(); }

    BoundingBox GetBoundingAABB() const {
        BoundingBox box;
        box.Expand(base);
        box.Expand(tip);
        Vector3 r(radius, radius, radius);
        box.min = box.min - r;
        box.max = box.max + r;
        return box;
    }
};

struct PenetrationResult {
    bool hit = false;
    Vector3 normal;
    float depth = 0.0f;
};

// Closest point on line segment AB to point P
Vector3 ClosestPointOnSegment(const Vector3& a, const Vector3& b, const Vector3& p);

// Closest point on triangle to point P (barycentric method)
Vector3 ClosestPointOnTriangle(const Triangle& tri, const Vector3& p);

// Moller-Trumbore ray-triangle intersection
std::optional<float> RayTriangleIntersect(const Vector3& origin, const Vector3& dir, const Triangle& tri);

// Capsule vs Triangle penetration test (Wicked Engine method)
PenetrationResult CapsuleTriangleIntersect(const Capsule& capsule, const Triangle& tri);

} // namespace UnoEngine
