#pragma once

#include "../Graphics/Mesh.h"
#include <string>
#include <vector>

namespace UnoEngine {

class GraphicsDevice;

struct StaticModelData {
    std::vector<Mesh> meshes;
};

class StaticModelImporter {
public:
    static StaticModelData Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                const std::string& filepath);

private:
    StaticModelImporter() = delete;
};

} // namespace UnoEngine
