#pragma once

#include "../Graphics/SkinnedMesh.h"
#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include "../Math/Vector.h"
#include <string>
#include <vector>
#include <memory>

namespace UnoEngine {

class GraphicsDevice;

struct SkinnedModelData {
    std::vector<SkinnedMesh> meshes;
    std::shared_ptr<Skeleton> skeleton;
    std::vector<std::shared_ptr<AnimationClip>> animations;
    struct BoundingBox {
        Vector3 min{ 0.0f, 0.0f, 0.0f };
        Vector3 max{ 0.0f, 0.0f, 0.0f };
    } boundingBox;
};

class SkinnedModelImporter {
public:
    static SkinnedModelData Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                 const std::string& filepath);

private:
    SkinnedModelImporter() = delete;
};

} // namespace UnoEngine
