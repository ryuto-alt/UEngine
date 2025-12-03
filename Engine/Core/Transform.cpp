#include "Transform.h"
#include <algorithm>

namespace UnoEngine {

void Transform::SetLocalPosition(const Vector3& pos) {
    localPosition_ = pos;
    MarkDirty();
}

void Transform::SetLocalRotation(const Quaternion& rot) {
    localRotation_ = rot;
    MarkDirty();
}

void Transform::SetLocalScale(const Vector3& scale) {
    localScale_ = scale;
    MarkDirty();
}

Vector3 Transform::GetPosition() const {
    // 親がない場合はローカル位置がそのままワールド位置
    if (!parent_) {
        return localPosition_;
    }
    // 親がある場合はワールド行列から取得
    if (isDirty_) UpdateWorldMatrix();
    // DirectXの行優先行列：Translation は m[3][0], m[3][1], m[3][2]
    return Vector3(
        cachedWorldMatrix_.GetElement(3, 0),
        cachedWorldMatrix_.GetElement(3, 1),
        cachedWorldMatrix_.GetElement(3, 2)
    );
}

Quaternion Transform::GetRotation() const {
    if (parent_) {
        return parent_->GetRotation() * localRotation_;
    }
    return localRotation_;
}

Vector3 Transform::GetScale() const {
    if (parent_) {
        Vector3 parentScale = parent_->GetScale();
        return Vector3(
            localScale_.GetX() * parentScale.GetX(),
            localScale_.GetY() * parentScale.GetY(),
            localScale_.GetZ() * parentScale.GetZ()
        );
    }
    return localScale_;
}

Matrix4x4 Transform::GetWorldMatrix() const {
    if (isDirty_) UpdateWorldMatrix();
    return cachedWorldMatrix_;
}

void Transform::SetPosition(const Vector3& pos) {
    if (parent_) {
        Matrix4x4 parentWorld = parent_->GetWorldMatrix();
        Matrix4x4 parentInv = parentWorld.Inverse();
        Vector3 localPos = parentInv.TransformPoint(pos);
        SetLocalPosition(localPos);
    } else {
        SetLocalPosition(pos);
    }
}

void Transform::SetRotation(const Quaternion& rot) {
    if (parent_) {
        Quaternion parentRot = parent_->GetRotation();
        Quaternion localRot = parentRot.Inverse() * rot;
        SetLocalRotation(localRot);
    } else {
        SetLocalRotation(rot);
    }
}

void Transform::SetParent(Transform* parent) {
    if (parent_ == parent) return;

    // Remove from old parent
    if (parent_) {
        auto& siblings = parent_->children_;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    // Add to new parent
    parent_ = parent;
    if (parent_) {
        parent_->children_.push_back(this);
    }

    MarkDirty();
}

Vector3 Transform::GetForward() const {
    Quaternion rot = GetRotation();
    return rot * Vector3(0.0f, 0.0f, 1.0f);
}

Vector3 Transform::GetRight() const {
    Quaternion rot = GetRotation();
    return rot * Vector3(1.0f, 0.0f, 0.0f);
}

Vector3 Transform::GetUp() const {
    Quaternion rot = GetRotation();
    return rot * Vector3(0.0f, 1.0f, 0.0f);
}

void Transform::MarkDirty() {
    isDirty_ = true;
    for (Transform* child : children_) {
        child->MarkDirty();
    }
}

void Transform::UpdateWorldMatrix() const {
    // DirectXの行優先行列で、点は右から掛ける: point * Matrix
    // 適用順序: Scale -> Rotate -> Translate
    // 行列の掛け算: S * R * T (右から左に適用される)
    Matrix4x4 localMatrix =
        Matrix4x4::Scale(localScale_) *
        localRotation_.ToMatrix() *
        Matrix4x4::Translation(localPosition_);

    if (parent_) {
        cachedWorldMatrix_ = localMatrix * parent_->GetWorldMatrix();
    } else {
        cachedWorldMatrix_ = localMatrix;
    }

    isDirty_ = false;
}

} // namespace UnoEngine
