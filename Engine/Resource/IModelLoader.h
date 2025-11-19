#pragma once

#include "../Graphics/Mesh.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Core/Types.h"
#include <vector>
#include <string>
#include <memory>

namespace UnoEngine {

/// モデルデータ（複数メッシュ、将来的にアニメーション情報も含む）
struct ModelData {
    std::vector<UniquePtr<Mesh>> meshes;
    std::string name;
    
    // Phase 2以降で追加予定
    // std::vector<AnimationClip> animations;
    // Skeleton skeleton;
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
