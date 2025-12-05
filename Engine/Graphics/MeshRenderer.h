#pragma once

#include "../Core/Component.h"
#include "Mesh.h"
#include "Material.h"
#include <string>
#include <vector>

namespace UnoEngine {

struct StaticModelData;

class MeshRenderer : public Component {
public:
    MeshRenderer() = default;
    MeshRenderer(Mesh* mesh, Material* material);

    void SetMesh(Mesh* mesh) { mesh_ = mesh; }
    void SetMaterial(Material* material) { material_ = material; }

    Mesh* GetMesh() const { return mesh_; }
    Material* GetMaterial() const { return material_; }

    // 複数メッシュ対応（StaticModelData用）
    void SetModel(StaticModelData* modelData) { modelData_ = modelData; }
    StaticModelData* GetModel() const { return modelData_; }
    bool HasModel() const;
    const std::vector<Mesh>& GetMeshes() const;

    // モデルパス（シリアライズ用）
    void SetModelPath(const std::string& path) { modelPath_ = path; }
    const std::string& GetModelPath() const { return modelPath_; }

private:
    Mesh* mesh_ = nullptr;
    Material* material_ = nullptr;
    StaticModelData* modelData_ = nullptr;
    std::string modelPath_;
};

} // namespace UnoEngine
