#pragma once

#include "IModelLoader.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/GraphicsDevice.h"
#include <string>

namespace UnoEngine {

/// OBJフォーマットのモデルローダー
class ObjLoader : public IModelLoader {
public:
    ObjLoader() = default;
    ~ObjLoader() override = default;

    /// OBJファイルを読み込む
    ModelData Load(GraphicsDevice* graphics,
                   ID3D12GraphicsCommandList* commandList,
                   const std::string& filepath) override;
    
    /// 後方互換性のため、単一Meshを返す静的メソッドも残す
    static Mesh LoadSingle(GraphicsDevice* graphics,
                          ID3D12GraphicsCommandList* commandList,
                          const std::string& filepath);

private:
    /// OBJファイルの実際のパース処理
    static Mesh ParseObjFile(GraphicsDevice* graphics,
                            ID3D12GraphicsCommandList* commandList,
                            const std::string& filepath);
};

} // namespace UnoEngine
