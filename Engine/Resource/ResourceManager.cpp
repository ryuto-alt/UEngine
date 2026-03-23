#include "pch.h"
#include "ResourceManager.h"
#include "ModelCache.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <chrono>
#include <filesystem>

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
    auto start = std::chrono::high_resolution_clock::now();

    if (!isUploading_) {
        Logger::Warning("ResourceManager: BeginUpload() not called before loading resources");
    }

    auto* commandList = device_->GetCommandList();
    auto modelData = std::make_unique<SkinnedModelData>();

    // バイナリキャッシュから読み込みを試行
    bool fromCache = false;
    if (ModelCache::IsEnabled()) {
        fromCache = ModelCache::TryLoadSkinnedModel(path, device_, commandList, *modelData);
    }

    if (!fromCache) {
        *modelData = SkinnedModelImporter::Load(device_, commandList, path);

        // キャッシュに保存
        if (ModelCache::IsEnabled() && !modelData->meshes.empty()) {
            ModelCache::SaveSkinnedModel(path, *modelData);
        }
    }

    if (modelData->meshes.empty()) {
        Logger::Error("[リソース] スキンモデル読み込み失敗: {}", path);
        return nullptr;
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    SkinnedModelData* ptr = modelData.get();
    skinnedModels_[path] = std::move(modelData);

    Logger::Info("[リソース] スキンモデル読み込み完了 (メッシュ: {}個, アニメーション: {}個) [{}ms{}]",
                 ptr->meshes.size(), ptr->animations.size(), ms, fromCache ? " キャッシュ" : "");

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
    auto start = std::chrono::high_resolution_clock::now();

    if (!isUploading_) {
        Logger::Warning("ResourceManager: BeginUpload() not called before loading resources");
    }

    auto* commandList = device_->GetCommandList();
    auto modelData = std::make_unique<StaticModelData>();

    // バイナリキャッシュから読み込みを試行
    bool fromCache = false;
    if (ModelCache::IsEnabled()) {
        fromCache = ModelCache::TryLoadStaticModel(path, device_, commandList, *modelData);
    }

    if (!fromCache) {
        *modelData = StaticModelImporter::Load(device_, commandList, path);

        // キャッシュに保存
        if (ModelCache::IsEnabled() && !modelData->meshes.empty()) {
            ModelCache::SaveStaticModel(path, *modelData);
        }
    }

    if (modelData->meshes.empty()) {
        Logger::Error("[リソース] 静的モデル読み込み失敗: {}", path);
        return nullptr;
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    StaticModelData* ptr = modelData.get();
    staticModels_[path] = std::move(modelData);

    Logger::Info("[リソース] 静的モデル読み込み完了 (メッシュ: {}個) [{}ms{}]",
                 ptr->meshes.size(), ms, fromCache ? " キャッシュ" : "");

    return ptr;
}

bool ResourceManager::LoadModel(const std::string& path, SkinnedModelData** outSkinnedModel, StaticModelData** outStaticModel) {
    if (outSkinnedModel) *outSkinnedModel = nullptr;
    if (outStaticModel) *outStaticModel = nullptr;

    // Check in-memory cache first
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

    // モデルタイプ判定キャッシュ: バイナリキャッシュファイルの存在でタイプを判定
    // これにより ModelHasBones() のAssimp二重パースを回避
    if (ModelCache::IsEnabled()) {
        namespace fs = std::filesystem;
        // スキンモデルキャッシュが存在するか
        size_t hash = std::hash<std::string>{}(fs::absolute(path).string());
        std::string stem = fs::path(path).stem().string();
        std::string skCachePath = ".cache/models/" + stem + "_" + std::to_string(hash) + ".skcache";
        std::string smCachePath = ".cache/models/" + stem + "_" + std::to_string(hash) + ".smcache";

        if (fs::exists(skCachePath) && ModelCache::IsCacheValid(path, skCachePath)) {
            Logger::Info("[リソース] キャッシュからスキンモデルとして判定");
            auto* skinnedModel = LoadSkinnedModel(path);
            if (outSkinnedModel) *outSkinnedModel = skinnedModel;
            return true;
        }
        if (fs::exists(smCachePath) && ModelCache::IsCacheValid(path, smCachePath)) {
            Logger::Info("[リソース] キャッシュから静的モデルとして判定");
            auto* staticModel = LoadStaticModel(path);
            if (outStaticModel) *outStaticModel = staticModel;
            return false;
        }
    }

    // キャッシュなし: Assimpでタイプ判定（初回のみ）
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

    Logger::Warning("ResourceManager: Standalone animation loading not yet implemented: {}", path);
    return nullptr;
}

void ResourceManager::UnloadUnused() {
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

    // GPU転送完了後にアップロードバッファを解放（GPUメモリリーク防止）
    ReleaseUploadBuffers();
}

void ResourceManager::ReleaseUploadBuffers() {
    // テクスチャのアップロードバッファ解放
    for (auto& [path, texture] : textures_) {
        if (texture) {
            texture->ReleaseUploadBuffer();
        }
    }

    // スキンモデルのアップロードバッファ解放 + CPU頂点データ解放
    for (auto& [path, model] : skinnedModels_) {
        if (model) {
            for (auto& mesh : model->meshes) {
                mesh.ReleaseUploadBuffers();
                mesh.ReleaseCPUData();
            }
        }
    }

    // 静的モデルのアップロードバッファ解放
    for (auto& [path, model] : staticModels_) {
        if (model) {
            for (auto& mesh : model->meshes) {
                mesh.ReleaseUploadBuffers();
            }
        }
    }

    Logger::Debug("[ResourceManager] アップロードバッファ解放完了");
}

} // namespace UnoEngine
