#include "pch.h"
#include "ResourceManager.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace UnoEngine {

namespace {

// Check if model has bones (skeleton)
bool ModelHasBones(const std::string& path) {
    Assimp::Importer importer;
    // Only load metadata, no heavy processing
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);

    if (!scene || !scene->mRootNode) {
        return false;
    }

    // Check if any mesh has bones
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        if (scene->mMeshes[i]->HasBones()) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

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
    Logger::Info("[リソース] スキンモデル読み込み中: {}", path);
    
    if (!isUploading_) {
        Logger::Warning("ResourceManager: BeginUpload() not called before loading resources");
    }

    auto* commandList = device_->GetCommandList();
    auto modelData = std::make_unique<SkinnedModelData>();
    *modelData = SkinnedModelImporter::Load(device_, commandList, path);

    if (modelData->meshes.empty()) {
        Logger::Error("[リソース] スキンモデル読み込み失敗: {}", path);
        return nullptr;
    }

    SkinnedModelData* ptr = modelData.get();
    skinnedModels_[path] = std::move(modelData);

    Logger::Info("[リソース] スキンモデル読み込み完了 (メッシュ: {}個, アニメーション: {}個)",
                 ptr->meshes.size(), ptr->animations.size());

    return ptr;
}

StaticModelData* ResourceManager::LoadStaticModel(const std::string& path) {
    // Check cache
    auto it = staticModels_.find(path);
    if (it != staticModels_.end()) {
        Logger::Debug("ResourceManager: Using cached static model: {}", path);
        return it->second.get();
    }

    // Load new model
    Logger::Info("[リソース] 静的モデル読み込み中: {}", path);

    if (!isUploading_) {
        Logger::Warning("ResourceManager: BeginUpload() not called before loading resources");
    }

    auto* commandList = device_->GetCommandList();
    auto modelData = std::make_unique<StaticModelData>();
    *modelData = StaticModelImporter::Load(device_, commandList, path);

    if (modelData->meshes.empty()) {
        Logger::Error("[リソース] 静的モデル読み込み失敗: {}", path);
        return nullptr;
    }

    StaticModelData* ptr = modelData.get();
    staticModels_[path] = std::move(modelData);

    Logger::Info("[リソース] 静的モデル読み込み完了 (メッシュ: {}個)", ptr->meshes.size());

    return ptr;
}

bool ResourceManager::LoadModel(const std::string& path, SkinnedModelData** outSkinnedModel, StaticModelData** outStaticModel) {
    if (outSkinnedModel) *outSkinnedModel = nullptr;
    if (outStaticModel) *outStaticModel = nullptr;

    // Check cache first
    auto skinnedIt = skinnedModels_.find(path);
    if (skinnedIt != skinnedModels_.end()) {
        if (outSkinnedModel) *outSkinnedModel = skinnedIt->second.get();
        return true;
    }

    auto staticIt = staticModels_.find(path);
    if (staticIt != staticModels_.end()) {
        if (outStaticModel) *outStaticModel = staticIt->second.get();
        return false;
    }

    // Check if model has bones
    Logger::Info("[リソース] モデルタイプを判定中: {}", path);
    bool hasBones = ModelHasBones(path);

    if (hasBones) {
        Logger::Info("[リソース] スキンモデルとして読み込みます");
        auto* skinnedModel = LoadSkinnedModel(path);
        if (outSkinnedModel) *outSkinnedModel = skinnedModel;
        return true;
    } else {
        Logger::Info("[リソース] 静的モデルとして読み込みます");
        auto* staticModel = LoadStaticModel(path);
        if (outStaticModel) *outStaticModel = staticModel;
        return false;
    }
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
    staticModels_.clear();
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
