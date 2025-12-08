#include "pch.h"
#include "NavMeshSerializer.h"
#include <fstream>
#include <cstring>

namespace UnoEngine
{

bool NavMeshSerializer::Save(const NavMeshData& navMesh, const std::filesystem::path& filePath)
{
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;
    
    // ヘッダー書き込み
    FileHeader header;
    header.vertexCount = static_cast<uint32_t>(navMesh.vertices.size());
    header.polygonCount = static_cast<uint32_t>(navMesh.polygons.size());
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // 設定を書き込み
    file.write(reinterpret_cast<const char*>(&navMesh.config), sizeof(NavMeshConfig));
    
    // バウンディングボックス
    file.write(reinterpret_cast<const char*>(&navMesh.bounds), sizeof(DirectX::BoundingBox));
    
    // 頂点データ
    if (!navMesh.vertices.empty())
    {
        file.write(reinterpret_cast<const char*>(navMesh.vertices.data()),
                   navMesh.vertices.size() * sizeof(DirectX::XMFLOAT3));
    }
    
    // ポリゴンデータ
    for (const auto& poly : navMesh.polygons)
    {
        // 頂点インデックス数
        uint32_t vertexCount = static_cast<uint32_t>(poly.vertexIndices.size());
        file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        
        // 頂点インデックス
        if (!poly.vertexIndices.empty())
        {
            file.write(reinterpret_cast<const char*>(poly.vertexIndices.data()),
                       poly.vertexIndices.size() * sizeof(uint32_t));
        }
        
        // 隣接ポリゴン数（頂点数と同じはず）
        uint32_t neighborCount = static_cast<uint32_t>(poly.neighbors.size());
        file.write(reinterpret_cast<const char*>(&neighborCount), sizeof(neighborCount));
        
        // 隣接ポリゴンID
        if (!poly.neighbors.empty())
        {
            file.write(reinterpret_cast<const char*>(poly.neighbors.data()),
                       poly.neighbors.size() * sizeof(uint32_t));
        }
        
        // 中心座標
        file.write(reinterpret_cast<const char*>(&poly.center), sizeof(DirectX::XMFLOAT3));
        
        // 面積
        file.write(reinterpret_cast<const char*>(&poly.area), sizeof(float));
        
        // フラグ
        file.write(reinterpret_cast<const char*>(&poly.flags), sizeof(uint16_t));
    }
    
    return file.good();
}

std::unique_ptr<NavMeshData> NavMeshSerializer::Load(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return nullptr;
    
    // ヘッダー読み込み
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    // マジックナンバー確認
    if (std::memcmp(header.magic, "NAVM", 4) != 0)
        return nullptr;
    
    // バージョン確認
    if (header.version > CURRENT_VERSION)
        return nullptr;
    
    auto navMesh = std::make_unique<NavMeshData>();
    
    // 設定読み込み
    file.read(reinterpret_cast<char*>(&navMesh->config), sizeof(NavMeshConfig));
    
    // バウンディングボックス
    file.read(reinterpret_cast<char*>(&navMesh->bounds), sizeof(DirectX::BoundingBox));
    
    // 頂点データ
    navMesh->vertices.resize(header.vertexCount);
    if (header.vertexCount > 0)
    {
        file.read(reinterpret_cast<char*>(navMesh->vertices.data()),
                  header.vertexCount * sizeof(DirectX::XMFLOAT3));
    }
    
    // ポリゴンデータ
    navMesh->polygons.resize(header.polygonCount);
    for (uint32_t i = 0; i < header.polygonCount; ++i)
    {
        auto& poly = navMesh->polygons[i];
        
        // 頂点インデックス数
        uint32_t vertexCount = 0;
        file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        
        // 頂点インデックス
        poly.vertexIndices.resize(vertexCount);
        if (vertexCount > 0)
        {
            file.read(reinterpret_cast<char*>(poly.vertexIndices.data()),
                      vertexCount * sizeof(uint32_t));
        }
        
        // 隣接ポリゴン数
        uint32_t neighborCount = 0;
        file.read(reinterpret_cast<char*>(&neighborCount), sizeof(neighborCount));
        
        // 隣接ポリゴンID
        poly.neighbors.resize(neighborCount);
        if (neighborCount > 0)
        {
            file.read(reinterpret_cast<char*>(poly.neighbors.data()),
                      neighborCount * sizeof(uint32_t));
        }
        
        // 中心座標
        file.read(reinterpret_cast<char*>(&poly.center), sizeof(DirectX::XMFLOAT3));
        
        // 面積
        file.read(reinterpret_cast<char*>(&poly.area), sizeof(float));
        
        // フラグ
        file.read(reinterpret_cast<char*>(&poly.flags), sizeof(uint16_t));
    }
    
    if (!file.good())
        return nullptr;
    
    return navMesh;
}

bool NavMeshSerializer::IsValidNavMeshFile(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
        return false;
    
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    return file.good() && std::memcmp(header.magic, "NAVM", 4) == 0;
}

} // namespace UnoEngine
