#pragma once

#include "../Graphics/Mesh.h"
#include "../Graphics/Material.h"
#include "../Math/Matrix.h"

namespace UnoEngine {

struct RenderItem {
    Mesh* mesh = nullptr;
    Material* material = nullptr;
    Matrix4x4 worldMatrix;

    RenderItem() = default;
    RenderItem(Mesh* m, Material* mat, const Matrix4x4& world)
        : mesh(m), material(mat), worldMatrix(world) {}
};

} // namespace UnoEngine
