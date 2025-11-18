#pragma once

#include "../Math/Math.h"
#include <vector>
#include <memory>

namespace UnoEngine {

class Transform {
public:
    Transform() = default;
    ~Transform() = default;

    // Local transform
    void SetLocalPosition(const Vector3& pos);
    void SetLocalRotation(const Quaternion& rot);
    void SetLocalScale(const Vector3& scale);

    Vector3 GetLocalPosition() const { return localPosition_; }
    Quaternion GetLocalRotation() const { return localRotation_; }
    Vector3 GetLocalScale() const { return localScale_; }

    // World transform
    Vector3 GetPosition() const;
    Quaternion GetRotation() const;
    Vector3 GetScale() const;
    Matrix4x4 GetWorldMatrix() const;

    void SetPosition(const Vector3& pos);
    void SetRotation(const Quaternion& rot);

    // Hierarchy
    void SetParent(Transform* parent);
    Transform* GetParent() const { return parent_; }
    const std::vector<Transform*>& GetChildren() const { return children_; }

    // Direction vectors
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;

private:
    void MarkDirty();
    void UpdateWorldMatrix() const;

    Vector3 localPosition_ = Vector3::Zero();
    Quaternion localRotation_ = Quaternion::Identity();
    Vector3 localScale_ = Vector3::One();

    Transform* parent_ = nullptr;
    std::vector<Transform*> children_;

    mutable Matrix4x4 cachedWorldMatrix_ = Matrix4x4::Identity();
    mutable bool isDirty_ = true;
};

} // namespace UnoEngine
