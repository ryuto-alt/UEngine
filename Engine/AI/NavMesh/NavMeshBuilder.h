#pragma once

#include "NavMeshTypes.h"
#include "../../Core/CollisionComponent.h"
#include <memory>
#include <functional>

namespace UnoEngine
{

class Scene;

// 入力ジオメトリ（三角形リスト）
struct NavMeshInputGeometry
{
    std::vector<DirectX::XMFLOAT3> vertices;
    std::vector<uint32_t> indices; // 三角形インデックス（3つで1三角形）
    
    void AddTriangle(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2);
    void AddAABB(const AABB& aabb);
    void Clear() { vertices.clear(); indices.clear(); }
    bool IsEmpty() const { return vertices.empty() || indices.empty(); }
};

// NavMesh生成クラス
class NavMeshBuilder
{
public:
    using ProgressCallback = std::function<void(float progress, const char* stage)>;
    
    NavMeshBuilder() = default;
    ~NavMeshBuilder() = default;
    
    // シーンのコライダーからNavMeshを生成
    std::unique_ptr<NavMeshData> Build(Scene* scene, const NavMeshConfig& config);
    
    // 入力ジオメトリからNavMeshを生成
    std::unique_ptr<NavMeshData> Build(const NavMeshInputGeometry& geometry, const NavMeshConfig& config);
    
    // 進捗コールバック設定
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = std::move(callback); }
    
private:
    // ビルドステップ
    void CollectGeometryFromScene(Scene* scene, NavMeshInputGeometry& outGeometry);
    void CollectGeometryFromCollider(CollisionComponent* collider, NavMeshInputGeometry& outGeometry);
    
    bool Voxelize(const NavMeshInputGeometry& geometry, const NavMeshConfig& config, HeightField& outHeightField);
    void FilterWalkableLowHeightSpans(HeightField& heightField, float walkableHeight);
    void FilterLedgeSpans(HeightField& heightField, float walkableClimb);
    
    bool BuildRegions(HeightField& heightField, const NavMeshConfig& config, std::vector<Region>& outRegions);
    bool BuildContours(const HeightField& heightField, const std::vector<Region>& regions, 
                       const NavMeshConfig& config, std::vector<Contour>& outContours);
    bool BuildPolygons(const std::vector<Contour>& contours, const NavMeshConfig& config, NavMeshData& outNavMesh);
    void BuildNeighborConnections(NavMeshData& navMesh);
    
    // ユーティリティ
    void ReportProgress(float progress, const char* stage);
    DirectX::XMFLOAT3 CalculatePolygonCenter(const NavMeshData& navMesh, const NavMeshPolygon& polygon);
    float CalculatePolygonArea(const NavMeshData& navMesh, const NavMeshPolygon& polygon);
    
    ProgressCallback m_progressCallback;
};

} // namespace UnoEngine
