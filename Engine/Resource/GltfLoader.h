#pragma once

#include "IModelLoader.h"
#include <string>

namespace UnoEngine {

/// glTF 2.0フォーマットのモデルローダー
class GltfLoader : public IModelLoader {
public:
    GltfLoader() = default;
    ~GltfLoader() override = default;

    /// glTFファイルを読み込む（.gltf / .glb対応）
    ModelData Load(GraphicsDevice* graphics,
                   ID3D12GraphicsCommandList* commandList,
                   const std::string& filepath) override;

private:
    /// tinygltfのモデルデータから頂点データを抽出
    std::vector<Vertex> ExtractVertices(const void* model, int meshIndex, int primitiveIndex);
    
    /// tinygltfのモデルデータからインデックスデータを抽出
    std::vector<uint32> ExtractIndices(const void* model, int meshIndex, int primitiveIndex);
    
    /// tinygltfのモデルデータからマテリアルデータを抽出
    MaterialData ExtractMaterial(const void* model, int materialIndex);
};

} // namespace UnoEngine
