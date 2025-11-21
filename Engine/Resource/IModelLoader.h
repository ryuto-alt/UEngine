#pragma once

#include "../Graphics/Mesh.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Rendering/Skeleton.h"
#include "../Rendering/Animation.h"
#include "../Core/Types.h"
#include <vector>
#include <string>
#include <memory>

namespace UnoEngine {

struct ModelData {
    std::vector<UniquePtr<Mesh>> meshes;
    std::vector<UniquePtr<SkinnedMesh>> skinnedMeshes;
    UniquePtr<Skeleton> skeleton;
    std::vector<UniquePtr<Animation>> animations;
    std::string name;
    bool hasSkin = false;
};

/// モデルローダーの共通インターフェース
class IModelLoader {
public:
    virtual ~IModelLoader() = default;
    
    /// モデルファイルを読み込む
    virtual ModelData Load(GraphicsDevice* graphics, 
                          ID3D12GraphicsCommandList* commandList,
                          const std::string& filepath) = 0;
};

} // namespace UnoEngine
