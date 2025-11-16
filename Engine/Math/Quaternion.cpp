#include "Quaternion.h"
#include "Matrix.h"

namespace UnoEngine {

Quaternion Quaternion::FromRotationMatrix(const Matrix4x4& mat) {
    return Quaternion(DirectX::XMQuaternionRotationMatrix(mat.GetXMMatrix()));
}

} // namespace UnoEngine
