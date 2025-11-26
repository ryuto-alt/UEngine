#include "SkinnedMesh.h"
#include "GraphicsDevice.h"
#include <algorithm>
#include <limits>

namespace UnoEngine {

void SkinnedMesh::Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                         const std::vector<SkinnedVertex>& vertices, const std::vector<uint32>& indices,
                         const std::string& name) {
    name_ = name;

    vertexBuffer_.Create(device, vertices.data(),
                        static_cast<uint32>(vertices.size() * sizeof(SkinnedVertex)),
                        sizeof(SkinnedVertex));
    indexBuffer_.Create(device, commandList, indices.data(),
                       static_cast<uint32>(indices.size()));

    CalculateBounds(vertices);
}

void SkinnedMesh::LoadMaterial(const MaterialData& materialData, GraphicsDevice* graphics,
                               ID3D12GraphicsCommandList* commandList,
                               const std::string& baseDirectory, uint32 srvIndex) {
    material_ = std::make_unique<Material>();
    material_->LoadFromData(materialData, graphics, commandList, baseDirectory, srvIndex);
}

void SkinnedMesh::CalculateBounds(const std::vector<SkinnedVertex>& vertices) {
    if (vertices.empty()) {
        boundsMin_ = Vector3(0.0f, 0.0f, 0.0f);
        boundsMax_ = Vector3(0.0f, 0.0f, 0.0f);
        return;
    }

    float minX = (std::numeric_limits<float>::max)();
    float minY = (std::numeric_limits<float>::max)();
    float minZ = (std::numeric_limits<float>::max)();
    float maxX = (std::numeric_limits<float>::lowest)();
    float maxY = (std::numeric_limits<float>::lowest)();
    float maxZ = (std::numeric_limits<float>::lowest)();

    for (const auto& v : vertices) {
        minX = (std::min)(minX, v.px);
        minY = (std::min)(minY, v.py);
        minZ = (std::min)(minZ, v.pz);
        maxX = (std::max)(maxX, v.px);
        maxY = (std::max)(maxY, v.py);
        maxZ = (std::max)(maxZ, v.pz);
    }

    boundsMin_ = Vector3(minX, minY, minZ);
    boundsMax_ = Vector3(maxX, maxY, maxZ);
}

} // namespace UnoEngine
