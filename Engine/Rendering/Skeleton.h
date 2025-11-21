#pragma once

#include "../Math/Math.h"
#include "../Core/Types.h"
#include <vector>
#include <string>

namespace UnoEngine {

struct Joint {
    std::string name;
    int32 parentIndex = -1;
    Vector3 translation = Vector3::Zero();
    Quaternion rotation = Quaternion::Identity();
    Vector3 scale = Vector3::One();
    Matrix4x4 inverseBindMatrix = Matrix4x4::Identity();
    Matrix4x4 localTransform = Matrix4x4::Identity();
    Matrix4x4 globalTransform = Matrix4x4::Identity();
};

class Skeleton {
public:
    Skeleton() = default;
    ~Skeleton() = default;

    void AddJoint(const Joint& joint);
    void ComputeGlobalTransforms();
    void ComputeJointMatrices(std::vector<Matrix4x4>& outMatrices) const;

    const std::vector<Joint>& GetJoints() const { return joints_; }
    std::vector<Joint>& GetJoints() { return joints_; }
    uint32 GetJointCount() const { return static_cast<uint32>(joints_.size()); }

    Joint* FindJointByName(const std::string& name);
    const Joint* FindJointByName(const std::string& name) const;

private:
    std::vector<Joint> joints_;
};

} // namespace UnoEngine
