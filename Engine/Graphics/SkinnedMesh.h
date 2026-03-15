#pragma once

#include "../Core/NonCopyable.h"
#include "../Core/Types.h"
#include "../Math/Vector.h"
#include "SkinnedVertex.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Material.h"
#include <vector>
#include <string>
#include <memory>
#include <d3d12.h>

namespace UnoEngine {

class GraphicsDevice;

class SkinnedMesh : public NonCopyable {
public:
    SkinnedMesh() = default;
    ~SkinnedMesh() = default;
    SkinnedMesh(SkinnedMesh&&) = default;
    SkinnedMesh& operator=(SkinnedMesh&&) = default;

    void Create(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                const std::vector<SkinnedVertex>& vertices, const std::vector<uint32>& indices,
                const std::string& name = "");

    void LoadMaterial(const MaterialData& materialData, GraphicsDevice* graphics,
                     ID3D12GraphicsCommandList* commandList,
                     const std::string& baseDirectory, uint32 srvIndex);

    const VertexBuffer& GetVertexBuffer() const { return vertexBuffer_; }
    const IndexBuffer& GetIndexBuffer() const { return indexBuffer_; }
    const std::string& GetName() const { return name_; }
    const Material* GetMaterial() const { return material_.get(); }
    bool HasMaterial() const { return material_ != nullptr; }

    Vector3 GetBoundsMin() const { return boundsMin_; }
    Vector3 GetBoundsMax() const { return boundsMax_; }

    // CPU-side geometry data for cache
    const std::vector<SkinnedVertex>& GetVertices() const { return cpuVertices_; }
    const std::vector<uint32>& GetIndices() const { return cpuIndices_; }

    // GPU転送完了後にアップロードバッファを解放（GPUメモリリーク防止）
    void ReleaseUploadBuffers() {
        vertexBuffer_.ReleaseUploadBuffer();
        indexBuffer_.ReleaseUploadBuffer();
        if (material_) {
            material_->ReleaseUploadBuffers();
        }
    }

private:
    void CalculateBounds(const std::vector<SkinnedVertex>& vertices);

    VertexBuffer vertexBuffer_;
    IndexBuffer indexBuffer_;
    std::string name_;
    Vector3 boundsMin_;
    Vector3 boundsMax_;
    std::unique_ptr<Material> material_;

    // CPU-side copies for model cache
    std::vector<SkinnedVertex> cpuVertices_;
    std::vector<uint32> cpuIndices_;
};

} // namespace UnoEngine
