#pragma once

#include "../Graphics/D3D12Common.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Core/Types.h"
#include <vector>
#include <string>

namespace UnoEngine {

// 草1本のインスタンスデータ（GPU送信用）
struct GrassInstance {
    float posX, posY, posZ;  // ワールド位置
    float rotation;           // Y軸回転（ラジアン）
    float scale;              // 高さスケール
    float colorVariation;     // 色変化 (0=暗め, 1=明るめ)
};

// 草の頂点データ（スター型クワッド）
struct GrassVertex {
    float px, py, pz;   // 位置
    float u, v;          // テクスチャ座標
    float heightFactor;  // 高さ係数 (0=根元, 1=先端) - 風アニメ用
};

// 草原データ管理
class GrassSystem {
public:
    GrassSystem() = default;
    ~GrassSystem() = default;

    // 初期化: 草メッシュ生成 + GPUバッファ作成
    void Initialize(GraphicsDevice* graphics);

    // インスタンス操作
    void AddInstance(float x, float y, float z, float rotation, float scale, float colorVar);
    void AddInstancesInRadius(float centerX, float centerY, float centerZ,
                              float radius, float density, float minScale, float maxScale, float colorVariation);
    void RemoveInstancesInRadius(float centerX, float centerY, float centerZ, float radius);
    void Clear();

    // GPUバッファ更新（インスタンスデータに変更があった場合）
    void UpdateInstanceBuffer(ID3D12GraphicsCommandList* commandList);

    // アクセサ
    uint32 GetInstanceCount() const { return static_cast<uint32>(instances_.size()); }
    const std::vector<GrassInstance>& GetInstances() const { return instances_; }
    std::vector<GrassInstance>& GetInstances() { return instances_; }

    // GPUリソース
    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return vertexBufferView_; }
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return indexBufferView_; }
    D3D12_VERTEX_BUFFER_VIEW GetInstanceBufferView() const { return instanceBufferView_; }
    uint32 GetIndexCount() const { return indexCount_; }

    bool IsDirty() const { return dirty_; }
    void SetDirty() { dirty_ = true; }

    // テクスチャパス（シリアライズ用メタデータ）
    const std::string& GetGrassTexturePath() const { return texturePath_; }
    void SetGrassTexturePath(const std::string& path) { texturePath_ = path; }

private:
    void CreateGrassMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void CreateInstanceBuffer(ID3D12Device* device);

    GraphicsDevice* graphics_ = nullptr;

    // 草メッシュ（共有ジオメトリ）
    ComPtr<ID3D12Resource> vertexBuffer_;
    ComPtr<ID3D12Resource> vertexUploadBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    ComPtr<ID3D12Resource> indexBuffer_;
    ComPtr<ID3D12Resource> indexUploadBuffer_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ = {};
    uint32 indexCount_ = 0;

    // インスタンスバッファ
    ComPtr<ID3D12Resource> instanceBuffer_;
    D3D12_VERTEX_BUFFER_VIEW instanceBufferView_ = {};
    static constexpr uint32 MAX_GRASS_INSTANCES = 200000;

    // CPU側インスタンスデータ
    std::vector<GrassInstance> instances_;
    bool dirty_ = false;
    std::string texturePath_;
};

} // namespace UnoEngine
