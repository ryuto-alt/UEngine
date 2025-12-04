#include "pch.h"
#include "Mesh.h"
#include "../Math/MathUtils.h"
#include <algorithm>
#include <cassert>

namespace UnoEngine {

void Mesh::Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                  const std::vector<Vertex>& vertices, const std::vector<uint32>& indices,
                  const std::string& name) {
    assert(device && commandList);
    assert(!vertices.empty() && !indices.empty());

    name_ = name;

    vertexBuffer_.Create(device, commandList, vertices.data(),
                        static_cast<uint32>(vertices.size() * sizeof(Vertex)),
                        sizeof(Vertex));

    indexBuffer_.Create(device, commandList, indices.data(),
                       static_cast<uint32>(indices.size()));

    CalculateBounds(vertices);
}


void Mesh::LoadMaterial(const MaterialData& materialData, GraphicsDevice* graphics,
                       ID3D12GraphicsCommandList* commandList,
                       const std::string& baseDirectory, uint32 srvIndex) {
    material_ = std::make_unique<Material>();
    material_->LoadFromData(materialData, graphics, commandList, baseDirectory, srvIndex);
}

void Mesh::CalculateBounds(const std::vector<Vertex>& vertices) {
    if (vertices.empty()) {
        boundsMin_ = boundsMax_ = Vector3::Zero();
        return;
    }

    float minX = vertices[0].px;
    float minY = vertices[0].py;
    float minZ = vertices[0].pz;
    float maxX = minX;
    float maxY = minY;
    float maxZ = minZ;

    for (const auto& vertex : vertices) {
        minX = (std::min)(minX, vertex.px);
        minY = (std::min)(minY, vertex.py);
        minZ = (std::min)(minZ, vertex.pz);
        maxX = (std::max)(maxX, vertex.px);
        maxY = (std::max)(maxY, vertex.py);
        maxZ = (std::max)(maxZ, vertex.pz);
    }

    boundsMin_ = Vector3(minX, minY, minZ);
    boundsMax_ = Vector3(maxX, maxY, maxZ);
}

} // namespace UnoEngine
