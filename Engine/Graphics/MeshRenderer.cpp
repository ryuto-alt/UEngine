#include "pch.h"
#include "MeshRenderer.h"
#include "../Resource/StaticModelImporter.h"

namespace UnoEngine {

MeshRenderer::MeshRenderer(Mesh* mesh, Material* material)
    : mesh_(mesh)
    , material_(material) {
}

bool MeshRenderer::HasModel() const {
    return modelData_ != nullptr && !modelData_->meshes.empty();
}

const std::vector<Mesh>& MeshRenderer::GetMeshes() const {
    static std::vector<Mesh> empty;
    return modelData_ ? modelData_->meshes : empty;
}

} // namespace UnoEngine
