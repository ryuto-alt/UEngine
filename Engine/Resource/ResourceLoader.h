#pragma once

#include "IModelLoader.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/Material.h"
#include "../Graphics/Texture2D.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace UnoEngine {

class GraphicsDevice;

/// リソースのキャッシュ管理と読み込みを行うクラス
class ResourceLoader {
public:
    static void Initialize(GraphicsDevice* graphics);
    static void Shutdown();

    /// メッシュを読み込む（OBJ/glTF対応、最初のメッシュを返す）
    static Mesh* LoadMesh(const std::string& path);
    
    /// モデルを読み込む（OBJ/glTF対応、全メッシュを返す）
    static ModelData* LoadModel(const std::string& path);
    
    static Material* LoadMaterial(const std::string& name);
    static Texture2D* LoadTexture(const std::wstring& path);

private:
    ResourceLoader() = default;
    static ResourceLoader& GetInstance();

    Mesh* LoadMeshImpl(const std::string& path);
    ModelData* LoadModelImpl(const std::string& path);
    Material* LoadMaterialImpl(const std::string& name);
    Texture2D* LoadTextureImpl(const std::wstring& path);

    void InitializeImpl(GraphicsDevice* graphics);
    void ShutdownImpl();

private:
    GraphicsDevice* graphics_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> meshCache_;
    std::unordered_map<std::string, std::unique_ptr<ModelData>> modelCache_;
    std::unordered_map<std::string, std::unique_ptr<Material>> materialCache_;
    std::unordered_map<std::wstring, std::unique_ptr<Texture2D>> textureCache_;
};

} // namespace UnoEngine
