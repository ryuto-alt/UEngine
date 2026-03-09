#include "pch.h"
#include "MeshColliderComponent.h"
#include "../Core/GameObject.h"
#include "../Graphics/MeshRenderer.h"
#include "../Resource/StaticModelImporter.h"
#include "../Core/Logger.h"

namespace UnoEngine {

void MeshColliderComponent::Awake() {
    RebuildBVH();
}

void MeshColliderComponent::RebuildBVH() {
    auto* meshRenderer = GetGameObject()->GetComponent<MeshRenderer>();
    if (!meshRenderer) {
        Logger::Warning("MeshColliderComponent: No MeshRenderer found on '{}'",
                       GetGameObject()->GetName());
        return;
    }

    bvh_ = std::make_unique<BVHTree>();

    if (meshRenderer->HasModel()) {
        // Multi-mesh model: concatenate all meshes
        const auto& meshes = meshRenderer->GetMeshes();
        std::vector<Vertex> allVertices;
        std::vector<uint32_t> allIndices;

        for (const auto& mesh : meshes) {
            if (!mesh.HasCPUData()) continue;
            uint32_t vertexOffset = static_cast<uint32_t>(allVertices.size());
            allVertices.insert(allVertices.end(),
                              mesh.GetVertices().begin(), mesh.GetVertices().end());
            for (uint32_t idx : mesh.GetIndices()) {
                allIndices.push_back(idx + vertexOffset);
            }
        }

        if (!allVertices.empty() && !allIndices.empty()) {
            bvh_->BuildFromMeshData(allVertices, allIndices);
            Logger::Info("MeshColliderComponent: Built BVH for '{}' ({} triangles)",
                        GetGameObject()->GetName(), bvh_->GetTriangleCount());
        }
    } else if (auto* mesh = meshRenderer->GetMesh()) {
        if (mesh->HasCPUData()) {
            bvh_->BuildFromMeshData(mesh->GetVertices(), mesh->GetIndices());
            Logger::Info("MeshColliderComponent: Built BVH for '{}' ({} triangles)",
                        GetGameObject()->GetName(), bvh_->GetTriangleCount());
        }
    }

    if (!bvh_->IsBuilt()) {
        Logger::Warning("MeshColliderComponent: Failed to build BVH for '{}' (no CPU data?)",
                       GetGameObject()->GetName());
    }
}

} // namespace UnoEngine
