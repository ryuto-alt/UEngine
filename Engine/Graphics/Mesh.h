#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "../Math/Vector.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include <vector>
#include <string>

namespace UnoEngine {

struct Vertex {
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
};

class Mesh : public NonCopyable {
public:
    Mesh() = default;
    ~Mesh() = default;
    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;

    void Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                const std::vector<Vertex>& vertices, const std::vector<uint32>& indices,
                const std::string& name = "");

    const VertexBuffer& GetVertexBuffer() const { return vertexBuffer_; }
    const IndexBuffer& GetIndexBuffer() const { return indexBuffer_; }
    const std::string& GetName() const { return name_; }

    Vector3 GetBoundsMin() const { return boundsMin_; }
    Vector3 GetBoundsMax() const { return boundsMax_; }

private:
    void CalculateBounds(const std::vector<Vertex>& vertices);

    VertexBuffer vertexBuffer_;
    IndexBuffer indexBuffer_;
    std::string name_;
    Vector3 boundsMin_;
    Vector3 boundsMax_;
};

} // namespace UnoEngine
