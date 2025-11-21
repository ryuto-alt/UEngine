#include "Skeleton.h"

namespace UnoEngine {

void Skeleton::AddJoint(const Joint& joint) {
    joints_.push_back(joint);
}

void Skeleton::ComputeGlobalTransforms() {
    for (size_t i = 0; i < joints_.size(); ++i) {
        auto& joint = joints_[i];

        Matrix4x4 T = Matrix4x4::Translation(joint.translation);
        Matrix4x4 R = Matrix4x4::RotationQuaternion(joint.rotation);
        Matrix4x4 S = Matrix4x4::Scaling(joint.scale);
        joint.localTransform = S * R * T;

        if (joint.parentIndex >= 0 && joint.parentIndex < static_cast<int32>(joints_.size())) {
            joint.globalTransform = joint.localTransform * joints_[joint.parentIndex].globalTransform;
        } else {
            joint.globalTransform = joint.localTransform;
        }
    }
}

void Skeleton::ComputeJointMatrices(std::vector<Matrix4x4>& outMatrices) const {
    outMatrices.resize(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        outMatrices[i] = joints_[i].inverseBindMatrix * joints_[i].globalTransform;
    }
}

Joint* Skeleton::FindJointByName(const std::string& name) {
    for (auto& joint : joints_) {
        if (joint.name == name) {
            return &joint;
        }
    }
    return nullptr;
}

const Joint* Skeleton::FindJointByName(const std::string& name) const {
    for (const auto& joint : joints_) {
        if (joint.name == name) {
            return &joint;
        }
    }
    return nullptr;
}

} // namespace UnoEngine
