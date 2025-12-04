#include "pch.h"
#include "ResourceLoader.h"
#include "../Graphics/GraphicsDevice.h"
#include "ModelImporter.h"
#include <stdexcept>

namespace UnoEngine {

void ResourceLoader::Initialize(GraphicsDevice* graphics) {
    GetInstance().InitializeImpl(graphics);
}

void ResourceLoader::Shutdown() {
    GetInstance().ShutdownImpl();
}

Mesh* ResourceLoader::LoadMesh(const std::string& path) {
    return GetInstance().LoadMeshImpl(path);
}

std::vector<Mesh*> ResourceLoader::LoadModel(const std::string& path) {
    return GetInstance().LoadModelImpl(path);
}

Material* ResourceLoader::LoadMaterial(const std::string& name) {
    return GetInstance().LoadMaterialImpl(name);
}

Texture2D* ResourceLoader::LoadTexture(const std::wstring& path) {
    return GetInstance().LoadTextureImpl(path);
}

ResourceLoader& ResourceLoader::GetInstance() {
    static ResourceLoader instance;
    return instance;
}

void ResourceLoader::InitializeImpl(GraphicsDevice* graphics) {
    graphics_ = graphics;
}

void ResourceLoader::ShutdownImpl() {
    meshCache_.clear();
    modelCache_.clear();
    materialCache_.clear();
    textureCache_.clear();
    graphics_ = nullptr;
}

Mesh* ResourceLoader::LoadMeshImpl(const std::string& path) {
    if (!graphics_) {
        throw std::runtime_error("ResourceLoader not initialized");
    }

    auto it = meshCache_.find(path);
    if (it != meshCache_.end()) {
        return it->second.get();
    }

    auto* device = graphics_->GetDevice();
    auto* commandQueue = graphics_->GetCommandQueue();
    auto* commandList = graphics_->GetCommandList();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    commandList->Reset(allocator.Get(), nullptr);

    std::vector<Mesh> loadedMeshes = ModelImporter::Load(graphics_, commandList, path);

    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, commandLists);

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    commandQueue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    CloseHandle(fenceEvent);

    if (loadedMeshes.empty()) {
        return nullptr;
    }

    auto mesh = std::make_unique<Mesh>(std::move(loadedMeshes[0]));
    Mesh* ptr = mesh.get();
    meshCache_[path] = std::move(mesh);
    return ptr;
}

std::vector<Mesh*> ResourceLoader::LoadModelImpl(const std::string& path) {
    if (!graphics_) {
        throw std::runtime_error("ResourceLoader not initialized");
    }

    auto it = modelCache_.find(path);
    if (it != modelCache_.end()) {
        std::vector<Mesh*> result;
        for (auto& mesh : it->second) {
            result.push_back(mesh.get());
        }
        return result;
    }

    auto* device = graphics_->GetDevice();
    auto* commandQueue = graphics_->GetCommandQueue();
    auto* commandList = graphics_->GetCommandList();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    commandList->Reset(allocator.Get(), nullptr);

    std::vector<Mesh> loadedMeshes = ModelImporter::Load(graphics_, commandList, path);

    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, commandLists);

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    commandQueue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    CloseHandle(fenceEvent);

    std::vector<Mesh*> result;
    std::vector<std::unique_ptr<Mesh>>& cacheEntry = modelCache_[path];

    for (auto& mesh : loadedMeshes) {
        auto uniqueMesh = std::make_unique<Mesh>(std::move(mesh));
        result.push_back(uniqueMesh.get());
        cacheEntry.push_back(std::move(uniqueMesh));
    }

    return result;
}

Material* ResourceLoader::LoadMaterialImpl(const std::string& name) {
    auto it = materialCache_.find(name);
    if (it != materialCache_.end()) {
        return it->second.get();
    }

    auto material = std::make_unique<Material>();
    Material* ptr = material.get();
    materialCache_[name] = std::move(material);
    return ptr;
}

Texture2D* ResourceLoader::LoadTextureImpl(const std::wstring& path) {
    if (!graphics_) {
        throw std::runtime_error("ResourceLoader not initialized");
    }

    auto it = textureCache_.find(path);
    if (it != textureCache_.end()) {
        return it->second.get();
    }

    auto* device = graphics_->GetDevice();
    auto* commandQueue = graphics_->GetCommandQueue();
    auto* commandList = graphics_->GetCommandList();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    commandList->Reset(allocator.Get(), nullptr);

    auto texture = std::make_unique<Texture2D>();

    static uint32 srvIndex = 0;
    texture->LoadFromFile(graphics_, commandList, path, srvIndex++);

    commandList->Close();
    ID3D12CommandList* commandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, commandLists);

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    commandQueue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    CloseHandle(fenceEvent);

    Texture2D* ptr = texture.get();
    textureCache_[path] = std::move(texture);
    return ptr;
}

} // namespace UnoEngine
