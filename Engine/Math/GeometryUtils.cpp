#include "pch.h"
#include "GeometryUtils.h"

namespace UnoEngine {

Vector3 ClosestPointOnSegment(const Vector3& a, const Vector3& b, const Vector3& p) {
    Vector3 ab = b - a;
    float lengthSq = ab.LengthSq();
    if (lengthSq < 1e-10f) return a;

    float t = std::clamp((p - a).Dot(ab) / lengthSq, 0.0f, 1.0f);
    return a + ab * t;
}

Vector3 ClosestPointOnTriangle(const Triangle& tri, const Vector3& p) {
    Vector3 ab = tri.v1 - tri.v0;
    Vector3 ac = tri.v2 - tri.v0;
    Vector3 ap = p - tri.v0;

    float d1 = ab.Dot(ap);
    float d2 = ac.Dot(ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return tri.v0;

    Vector3 bp = p - tri.v1;
    float d3 = ab.Dot(bp);
    float d4 = ac.Dot(bp);
    if (d3 >= 0.0f && d4 <= d3) return tri.v1;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return tri.v0 + ab * v;
    }

    Vector3 cp = p - tri.v2;
    float d5 = ab.Dot(cp);
    float d6 = ac.Dot(cp);
    if (d6 >= 0.0f && d5 <= d6) return tri.v2;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return tri.v0 + ac * w;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return tri.v1 + (tri.v2 - tri.v1) * w;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return tri.v0 + ab * v + ac * w;
}

std::optional<float> RayTriangleIntersect(const Vector3& origin, const Vector3& dir, const Triangle& tri) {
    constexpr float kEpsilon = 1e-6f;

    Vector3 edge1 = tri.v1 - tri.v0;
    Vector3 edge2 = tri.v2 - tri.v0;
    Vector3 h = dir.Cross(edge2);
    float a = edge1.Dot(h);

    if (std::abs(a) < kEpsilon) return std::nullopt;

    float f = 1.0f / a;
    Vector3 s = origin - tri.v0;
    float u = f * s.Dot(h);
    if (u < 0.0f || u > 1.0f) return std::nullopt;

    Vector3 q = s.Cross(edge1);
    float v = f * dir.Dot(q);
    if (v < 0.0f || u + v > 1.0f) return std::nullopt;

    float t = f * edge2.Dot(q);
    if (t > kEpsilon) return t;
    return std::nullopt;
}

PenetrationResult CapsuleTriangleIntersect(const Capsule& capsule, const Triangle& tri) {
    // Wicked Engine approach: project capsule segment onto triangle plane,
    // find closest point, then test sphere at closest capsule point

    Vector3 normal = tri.GetNormal();
    if (normal.LengthSq() < 1e-10f) return {};

    // Find the capsule segment point closest to the triangle plane
    float distBase = (capsule.base - tri.v0).Dot(normal);
    float distTip  = (capsule.tip  - tri.v0).Dot(normal);

    // Reference point on capsule closest to triangle plane
    Vector3 refPoint;
    if (distBase * distTip < 0.0f) {
        // Segment crosses the plane — interpolate
        float t = distBase / (distBase - distTip);
        refPoint = capsule.base + (capsule.tip - capsule.base) * t;
    } else {
        // Pick the endpoint closer to plane
        refPoint = (std::abs(distBase) < std::abs(distTip)) ? capsule.base : capsule.tip;
    }

    // Project reference point onto triangle plane
    float dist = (refPoint - tri.v0).Dot(normal);
    Vector3 projected = refPoint - normal * dist;

    // Find closest point on triangle to the projected point
    Vector3 closestOnTri = ClosestPointOnTriangle(tri, projected);

    // Find closest point on capsule segment to that triangle point
    Vector3 closestOnCapsule = ClosestPointOnSegment(capsule.base, capsule.tip, closestOnTri);

    // Sphere-point test
    Vector3 delta = closestOnCapsule - closestOnTri;
    float distSq = delta.LengthSq();
    float r = capsule.radius;

    if (distSq > r * r) return {};

    float distance = std::sqrt(distSq);

    PenetrationResult result;
    result.hit = true;
    result.depth = r - distance;

    if (distance > 1e-6f) {
        result.normal = delta * (1.0f / distance);
    } else {
        result.normal = normal;
    }

    return result;
}

} // namespace UnoEngine
