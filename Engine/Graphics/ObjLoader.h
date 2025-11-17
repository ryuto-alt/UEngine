#pragma once

#include "Mesh.h"
#include "GraphicsDevice.h"
#include <string>

namespace UnoEngine {

class ObjLoader {
public:
    static Mesh Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                    const std::string& filepath);

private:
    ObjLoader() = delete;
};

} // namespace UnoEngine
