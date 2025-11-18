#pragma once

#include "../Core/Component.h"
#include "Mesh.h"
#include "Material.h"

namespace UnoEngine {

class MeshRenderer : public Component {
public:
    MeshRenderer() = default;
    MeshRenderer(Mesh* mesh, Material* material);

    void SetMesh(Mesh* mesh) { mesh_ = mesh; }
    void SetMaterial(Material* material) { material_ = material; }

    Mesh* GetMesh() const { return mesh_; }
    Material* GetMaterial() const { return material_; }

private:
    Mesh* mesh_ = nullptr;
    Material* material_ = nullptr;
};

} // namespace UnoEngine
