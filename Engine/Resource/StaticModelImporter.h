#pragma once

#include "../Graphics/Mesh.h"
#include "../Math/Vector.h"
#include <string>
#include <vector>

namespace UnoEngine {

class GraphicsDevice;

struct StaticModelData {
    std::vector<Mesh> meshes;
    struct BoundingBox {
        Vector3 min{ 0.0f, 0.0f, 0.0f };
        Vector3 max{ 0.0f, 0.0f, 0.0f };
    } boundingBox;
};

class StaticModelImporter {
public:
    static StaticModelData Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                const std::string& filepath);

private:
    StaticModelImporter() = delete;
};

} // namespace UnoEngine
