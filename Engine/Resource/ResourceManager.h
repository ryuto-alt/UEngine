#pragma once

#include "../Core/Types.h"
#include "SkinnedModelImporter.h"
#include "StaticModelImporter.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/Material.h"
#include "../Animation/AnimationClip.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace UnoEngine {

class GraphicsDevice;

class ResourceManager {
public:
    explicit ResourceManager(GraphicsDevice* device);
    ~ResourceManager();

    // Skinned Model loading (cached) - auto-detects if model has bones
    // If model has no bones, returns nullptr (use LoadStaticModel instead)
    SkinnedModelData* LoadSkinnedModel(const std::string& path);

    // Static Model loading (cached) - for glTF/OBJ without skeleton
    StaticModelData* LoadStaticModel(const std::string& path);

    // Auto-detect model type and load appropriately
    // Returns true if skinned model was loaded, false if static model was loaded
    // outSkinnedModel or outStaticModel will be set based on model type
    bool LoadModel(const std::string& path, SkinnedModelData** outSkinnedModel, StaticModelData** outStaticModel);

    // Texture loading (cached)
    Texture2D* LoadTexture(const std::wstring& path);

    // Animation clip loading (cached)
    std::shared_ptr<AnimationClip> LoadAnimation(const std::string& path);

    // Cache management
    void UnloadUnused();
    void Clear();

    // Stats
    size_t GetSkinnedModelCount() const { return skinnedModels_.size(); }
    size_t GetStaticModelCount() const { return staticModels_.size(); }
    size_t GetTextureCount() const { return textures_.size(); }
    size_t GetAnimationCount() const { return animations_.size(); }

    // Begin/End resource upload batch
    void BeginUpload();
    void EndUpload();

private:
    GraphicsDevice* device_;

    std::unordered_map<std::string, std::unique_ptr<SkinnedModelData>> skinnedModels_;
    std::unordered_map<std::string, std::unique_ptr<StaticModelData>> staticModels_;
    std::unordered_map<std::wstring, std::unique_ptr<Texture2D>> textures_;
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> animations_;

    uint32 nextSrvIndex_ = 0;
    bool isUploading_ = false;
};

} // namespace UnoEngine
