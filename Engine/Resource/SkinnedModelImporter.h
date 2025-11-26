#pragma once

#include "../Graphics/SkinnedMesh.h"
#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include <string>
#include <vector>
#include <memory>

namespace UnoEngine {

class GraphicsDevice;

struct SkinnedModelData {
    std::vector<SkinnedMesh> meshes;
    std::shared_ptr<Skeleton> skeleton;
    std::vector<std::shared_ptr<AnimationClip>> animations;
};

class SkinnedModelImporter {
public:
    static SkinnedModelData Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                 const std::string& filepath);

private:
    SkinnedModelImporter() = delete;
};

} // namespace UnoEngine
