#include "ResourceManager.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Core/Logger.h"

namespace UnoEngine {

ResourceManager::ResourceManager(GraphicsDevice* device)
    : device_(device)
    , nextSrvIndex_(100) {  // Start after reserved indices
}

ResourceManager::~ResourceManager() {
    Clear();
}

SkinnedModelData* ResourceManager::LoadSkinnedModel(const std::string& path) {
    // Check cache
    auto it = skinnedModels_.find(path);
    if (it != skinnedModels_.end()) {
        Logger::Debug("ResourceManager: Using cached skinned model: {}", path);
        return it->second.get();
    }

    // Load new model
    Logger::Info("ResourceManager: Loading skinned model: {}", path);
    
    if (!isUploading_) {
        Logger::Warning("ResourceManager: BeginUpload() not called before loading resources");
    }

    auto* commandList = device_->GetCommandList();
    auto modelData = std::make_unique<SkinnedModelData>();
    *modelData = SkinnedModelImporter::Load(device_, commandList, path);

    if (modelData->meshes.empty()) {
        Logger::Error("ResourceManager: Failed to load skinned model: {}", path);
        return nullptr;
    }

    SkinnedModelData* ptr = modelData.get();
    skinnedModels_[path] = std::move(modelData);

    Logger::Info("ResourceManager: Loaded skinned model with {} meshes, {} animations",
                 ptr->meshes.size(), ptr->animations.size());

    return ptr;
}

Texture2D* ResourceManager::LoadTexture(const std::wstring& path) {
    // Check cache
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        return it->second.get();
    }

    // Load new texture
    if (!isUploading_) {
        Logger::Warning("ResourceManager: BeginUpload() not called before loading resources");
    }

    auto* commandList = device_->GetCommandList();
    auto texture = std::make_unique<Texture2D>();
    texture->LoadFromFile(device_, commandList, path, nextSrvIndex_++);

    Texture2D* ptr = texture.get();
    textures_[path] = std::move(texture);

    return ptr;
}

std::shared_ptr<AnimationClip> ResourceManager::LoadAnimation(const std::string& path) {
    // Check cache
    auto it = animations_.find(path);
    if (it != animations_.end()) {
        return it->second;
    }

    // Animation clips are typically loaded as part of SkinnedModel
    // This method is for loading standalone animation files (future feature)
    Logger::Warning("ResourceManager: Standalone animation loading not yet implemented: {}", path);
    return nullptr;
}

void ResourceManager::UnloadUnused() {
    // For now, just log. In the future, implement reference counting
    Logger::Debug("ResourceManager: UnloadUnused() called - not yet implemented");
}

void ResourceManager::Clear() {
    Logger::Info("ResourceManager: Clearing all cached resources");
    skinnedModels_.clear();
    textures_.clear();
    animations_.clear();
}

void ResourceManager::BeginUpload() {
    if (isUploading_) {
        Logger::Warning("ResourceManager: BeginUpload() called while already uploading");
        return;
    }
    device_->BeginResourceUpload();
    isUploading_ = true;
}

void ResourceManager::EndUpload() {
    if (!isUploading_) {
        Logger::Warning("ResourceManager: EndUpload() called without BeginUpload()");
        return;
    }
    device_->EndResourceUpload();
    isUploading_ = false;
}

} // namespace UnoEngine
