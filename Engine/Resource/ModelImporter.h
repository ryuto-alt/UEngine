#pragma once
#include "../Graphics/Mesh.h"
#include "../Graphics/GraphicsDevice.h"
#include <vector>
#include <string>

namespace UnoEngine {

class ModelImporter {
public:
    static std::vector<Mesh> Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                  const std::string& filepath);

private:
    ModelImporter() = delete;
};

} // namespace UnoEngine
