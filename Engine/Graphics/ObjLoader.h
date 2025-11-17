#pragma once

#include "Mesh.h"
#include <string>

namespace UnoEngine {

class ObjLoader {
public:
    static Mesh Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                    const std::string& filepath);

private:
    ObjLoader() = delete;
};

} // namespace UnoEngine
