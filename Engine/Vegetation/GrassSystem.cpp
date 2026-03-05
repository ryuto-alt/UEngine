#include "pch.h"
#include "GrassSystem.h"
#include "../Core/Logger.h"
#include <cmath>
#include <random>

namespace UnoEngine {

namespace {

constexpr float GRASS_WIDTH  = 0.15f;  // 片側幅（両側で0.3m）
constexpr float GRASS_HEIGHT = 0.4f;
constexpr int   QUAD_COUNT   = 3;      // スター型: 3枚のクワッド
constexpr float PI = 3.14159265359f;

} // anonymous namespace

void GrassSystem::Initialize(GraphicsDevice* graphics) {
    graphics_ = graphics;
    auto* device = graphics_->GetDevice();
    auto* cmdList = graphics_->GetCommandList();

    CreateGrassMesh(device, cmdList);
    CreateInstanceBuffer(device);

    Logger::Info("[GrassSystem] 初期化完了 (最大インスタンス: {})", MAX_GRASS_INSTANCES);
}

void GrassSystem::CreateGrassMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
    // 3枚のクワッドを0°, 60°, 120°で交差配置（スター型）
    // 各クワッド: 4頂点, 6インデックス（2三角形）
    std::vector<GrassVertex> vertices;
    std::vector<uint16> indices;

    vertices.reserve(QUAD_COUNT * 4);
    indices.reserve(QUAD_COUNT * 6);

    for (int q = 0; q < QUAD_COUNT; ++q) {
        float angle = static_cast<float>(q) * PI / 3.0f; // 0°, 60°, 120°
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        // クワッドのローカル座標（X方向に幅、Y方向に高さ）
        // 左下, 右下, 右上, 左上
        float lx = -GRASS_WIDTH, rx = GRASS_WIDTH;

        uint16 baseIdx = static_cast<uint16>(vertices.size());

        // 左下 (根元)
        vertices.push_back({ lx * cosA, 0.0f, lx * sinA,  0.0f, 1.0f, 0.0f });
        // 右下 (根元)
        vertices.push_back({ rx * cosA, 0.0f, rx * sinA,  1.0f, 1.0f, 0.0f });
        // 右上 (先端)
        vertices.push_back({ rx * cosA, GRASS_HEIGHT, rx * sinA,  1.0f, 0.0f, 1.0f });
        // 左上 (先端)
        vertices.push_back({ lx * cosA, GRASS_HEIGHT, lx * sinA,  0.0f, 0.0f, 1.0f });

        // 2三角形（表面）
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 1);

        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 3);
        indices.push_back(baseIdx + 2);
    }

    indexCount_ = static_cast<uint32>(indices.size());

    // 頂点バッファ作成
    const uint32 vertexBufferSize = static_cast<uint32>(vertices.size() * sizeof(GrassVertex));
    {
        D3D12_HEAP_PROPERTIES defaultHeap = {};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = vertexBufferSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(
            device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&vertexBuffer_)),
            "Failed to create grass vertex buffer");

        // アップロードバッファ
        D3D12_HEAP_PROPERTIES uploadHeap = {};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        ThrowIfFailed(
            device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexUploadBuffer_)),
            "Failed to create grass vertex upload buffer");

        void* mapped = nullptr;
        vertexUploadBuffer_->Map(0, nullptr, &mapped);
        memcpy(mapped, vertices.data(), vertexBufferSize);
        vertexUploadBuffer_->Unmap(0, nullptr);

        commandList->CopyBufferRegion(vertexBuffer_.Get(), 0, vertexUploadBuffer_.Get(), 0, vertexBufferSize);

        vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = vertexBufferSize;
        vertexBufferView_.StrideInBytes = sizeof(GrassVertex);
    }

    // インデックスバッファ作成
    const uint32 indexBufferSize = static_cast<uint32>(indices.size() * sizeof(uint16));
    {
        D3D12_HEAP_PROPERTIES defaultHeap = {};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = indexBufferSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(
            device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&indexBuffer_)),
            "Failed to create grass index buffer");

        D3D12_HEAP_PROPERTIES uploadHeap = {};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        ThrowIfFailed(
            device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexUploadBuffer_)),
            "Failed to create grass index upload buffer");

        void* mapped = nullptr;
        indexUploadBuffer_->Map(0, nullptr, &mapped);
        memcpy(mapped, indices.data(), indexBufferSize);
        indexUploadBuffer_->Unmap(0, nullptr);

        commandList->CopyBufferRegion(indexBuffer_.Get(), 0, indexUploadBuffer_.Get(), 0, indexBufferSize);

        indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = indexBufferSize;
        indexBufferView_.Format = DXGI_FORMAT_R16_UINT;
    }

    Logger::Debug("[GrassSystem] 草メッシュ作成: {}頂点, {}インデックス", vertices.size(), indices.size());
}

void GrassSystem::CreateInstanceBuffer(ID3D12Device* device) {
    const uint32 bufferSize = MAX_GRASS_INSTANCES * sizeof(GrassInstance);

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = bufferSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(
        device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&instanceBuffer_)),
        "Failed to create grass instance buffer");

    instanceBufferView_.BufferLocation = instanceBuffer_->GetGPUVirtualAddress();
    instanceBufferView_.SizeInBytes = bufferSize;
    instanceBufferView_.StrideInBytes = sizeof(GrassInstance);
}

void GrassSystem::AddInstance(float x, float y, float z, float rotation, float scale, float colorVar) {
    if (instances_.size() >= MAX_GRASS_INSTANCES) return;

    instances_.push_back({ x, y, z, rotation, scale, colorVar });
    dirty_ = true;
}

void GrassSystem::AddInstancesInRadius(float centerX, float centerY, float centerZ,
                                        float radius, float density, float minScale, float maxScale, float colorVariation) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> distRadius(0.0f, 1.0f);
    std::uniform_real_distribution<float> distRotation(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> distScale(minScale, maxScale);
    std::uniform_real_distribution<float> distColor(0.0f, colorVariation);

    // density = instances per m²
    float area = PI * radius * radius;
    int count = static_cast<int>(area * density);

    for (int i = 0; i < count; ++i) {
        if (instances_.size() >= MAX_GRASS_INSTANCES) break;

        // 均一なランダム分布（sqrt でバイアス補正）
        float r = radius * std::sqrt(distRadius(gen));
        float a = distAngle(gen);
        float x = centerX + r * std::cos(a);
        float z = centerZ + r * std::sin(a);

        AddInstance(x, centerY, z, distRotation(gen), distScale(gen), distColor(gen));
    }
}

void GrassSystem::RemoveInstancesInRadius(float centerX, float centerY, float centerZ, float radius) {
    float r2 = radius * radius;
    auto it = std::remove_if(instances_.begin(), instances_.end(),
        [&](const GrassInstance& inst) {
            float dx = inst.posX - centerX;
            float dz = inst.posZ - centerZ;
            return (dx * dx + dz * dz) <= r2;
        });

    if (it != instances_.end()) {
        instances_.erase(it, instances_.end());
        dirty_ = true;
    }
}

void GrassSystem::Clear() {
    instances_.clear();
    dirty_ = true;
}

void GrassSystem::UpdateInstanceBuffer(ID3D12GraphicsCommandList* /*commandList*/) {
    if (!dirty_ || instances_.empty()) return;

    void* mapped = nullptr;
    instanceBuffer_->Map(0, nullptr, &mapped);
    memcpy(mapped, instances_.data(), instances_.size() * sizeof(GrassInstance));
    instanceBuffer_->Unmap(0, nullptr);

    // InstanceBufferViewのサイズを実際のインスタンス数に合わせる
    instanceBufferView_.SizeInBytes = static_cast<uint32>(instances_.size() * sizeof(GrassInstance));

    dirty_ = false;
}

} // namespace UnoEngine
