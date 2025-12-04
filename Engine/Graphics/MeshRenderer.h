#pragma once

#include "../Core/Component.h"
#include "Mesh.h"
#include "Material.h"
#include <string>

namespace UnoEngine {

class MeshRenderer : public Component {
public:
    MeshRenderer() = default;
    MeshRenderer(Mesh* mesh, Material* material);

    void SetMesh(Mesh* mesh) { mesh_ = mesh; }
    void SetMaterial(Material* material) { material_ = material; }

    Mesh* GetMesh() const { return mesh_; }
    Material* GetMaterial() const { return material_; }

    // モデルパス（シリアライズ用）
    void SetModelPath(const std::string& path) { modelPath_ = path; }
    const std::string& GetModelPath() const { return modelPath_; }

private:
    Mesh* mesh_ = nullptr;
    Material* material_ = nullptr;
    std::string modelPath_;
};

} // namespace UnoEngine
