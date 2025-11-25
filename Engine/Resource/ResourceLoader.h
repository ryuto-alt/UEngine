#pragma once

#include "../Graphics/Mesh.h"
#include "../Graphics/Material.h"
#include "../Graphics/Texture2D.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace UnoEngine {

class GraphicsDevice;

class ResourceLoader {
public:
    static void Initialize(GraphicsDevice* graphics);
    static void Shutdown();

    static Mesh* LoadMesh(const std::string& path);
    static std::vector<Mesh*> LoadModel(const std::string& path);
    static Material* LoadMaterial(const std::string& name);
    static Texture2D* LoadTexture(const std::wstring& path);

private:
    ResourceLoader() = default;
    static ResourceLoader& GetInstance();

    Mesh* LoadMeshImpl(const std::string& path);
    std::vector<Mesh*> LoadModelImpl(const std::string& path);
    Material* LoadMaterialImpl(const std::string& name);
    Texture2D* LoadTextureImpl(const std::wstring& path);

    void InitializeImpl(GraphicsDevice* graphics);
    void ShutdownImpl();

private:
    GraphicsDevice* graphics_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> meshCache_;
    std::unordered_map<std::string, std::vector<std::unique_ptr<Mesh>>> modelCache_;
    std::unordered_map<std::string, std::unique_ptr<Material>> materialCache_;
    std::unordered_map<std::wstring, std::unique_ptr<Texture2D>> textureCache_;
};

} // namespace UnoEngine
