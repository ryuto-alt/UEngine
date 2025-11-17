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

    vertexBuffer_.Create(device, vertices.data(),
                        static_cast<uint32>(vertices.size() * sizeof(Vertex)),
                        sizeof(Vertex));

    indexBuffer_.Create(device, commandList, indices.data(),
                       static_cast<uint32>(indices.size()));

    CalculateBounds(vertices);
}

void Mesh::CalculateBounds(const std::vector<Vertex>& vertices) {
    if (vertices.empty()) {
        boundsMin_ = boundsMax_ = Vector3::Zero();
        return;
    }

    float minX = vertices[0].position.GetX();
    float minY = vertices[0].position.GetY();
    float minZ = vertices[0].position.GetZ();
    float maxX = minX;
    float maxY = minY;
    float maxZ = minZ;

    for (const auto& vertex : vertices) {
        const auto& pos = vertex.position;
        minX = (std::min)(minX, pos.GetX());
        minY = (std::min)(minY, pos.GetY());
        minZ = (std::min)(minZ, pos.GetZ());
        maxX = (std::max)(maxX, pos.GetX());
        maxY = (std::max)(maxY, pos.GetY());
        maxZ = (std::max)(maxZ, pos.GetZ());
    }

    boundsMin_ = Vector3(minX, minY, minZ);
    boundsMax_ = Vector3(maxX, maxY, maxZ);
}

} // namespace UnoEngine
