#pragma once

#include "IModelLoader.h"
#include <string>

namespace UnoEngine {

class Skeleton;
class Animation;

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

    /// tinygltfのモデルデータからスキニング頂点データを抽出
    std::vector<VertexSkinned> ExtractSkinnedVertices(const void* model, int meshIndex, int primitiveIndex);

    /// tinygltfのモデルデータからインデックスデータを抽出
    std::vector<uint32> ExtractIndices(const void* model, int meshIndex, int primitiveIndex);

    /// tinygltfのモデルデータからマテリアルデータを抽出
    MaterialData ExtractMaterial(const void* model, int materialIndex);

    /// tinygltfのモデルデータからスケルトンデータを抽出
    UniquePtr<Skeleton> ExtractSkeleton(const void* model, int skinIndex);

    /// tinygltfのモデルデータからアニメーションデータを抽出
    std::vector<UniquePtr<Animation>> ExtractAnimations(const void* model);

    /// プリミティブがスキニング属性を持っているかチェック
    bool HasSkinningAttributes(const void* model, int meshIndex, int primitiveIndex);
};

} // namespace UnoEngine
