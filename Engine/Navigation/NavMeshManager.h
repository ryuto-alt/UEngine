#pragma once
#pragma once

#include "NavMeshBuildSettings.h"

#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <string>
#include <functional>

// Forward declarations - Recast/Detour
class rcContext;
struct rcConfig;
struct rcHeightfield;
struct rcCompactHeightfield;
struct rcContourSet;
struct rcPolyMesh;
struct rcPolyMeshDetail;
class dtNavMesh;
class dtNavMeshQuery;

// Forward declarations - UnoEngine
namespace UnoEngine { class DebugRenderer; }

namespace UnoEngine::Navigation {

/// NavMesh用の静的ジオメトリデータ
struct StaticGeometry
{
    std::vector<DirectX::XMFLOAT3> vertices;
    std::vector<unsigned int> indices;
    DirectX::XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
};

/// NavMesh統計情報
struct NavMeshStats
{
    int polyCount = 0;
    int vertexCount = 0;
    size_t memoryUsage = 0;
    int tileCount = 0;
    float buildTimeSeconds = 0.0f;
};

/// Recast/Detour NavMesh Manager
/// 責務: NavMesh生成・管理・クエリ
class NavMeshManager
{
public:
    using ProgressCallback = std::function<void(float progress, const char* stage)>;

    static NavMeshManager& Get();
    
    // ========== Lifecycle ==========
    void Initialize();
    void Shutdown();
    
    // ========== Building ==========
    bool BuildNavMesh(const std::vector<StaticGeometry>& geometry,
                      const NavMeshBuildSettings& settings);
    
    // ========== Queries ==========
    [[nodiscard]] bool FindPath(const DirectX::XMFLOAT3& start, 
                                const DirectX::XMFLOAT3& goal,
                                std::vector<DirectX::XMFLOAT3>& outPath);
    
    [[nodiscard]] DirectX::XMFLOAT3 GetNextPathPoint(const DirectX::XMFLOAT3& currentPos, 
                                                      const DirectX::XMFLOAT3& goal,
                                                      float lookAheadDist = 1.0f) const;
    
    [[nodiscard]] bool IsPointOnNavMesh(const DirectX::XMFLOAT3& pos) const;
    
    // ========== I/O ==========
    bool SaveNavMesh(const std::string& filename) const;
    bool LoadNavMesh(const std::string& filename);
    
    // ========== Settings ==========
    void SetSettings(const NavMeshBuildSettings& settings);
    [[nodiscard]] const NavMeshBuildSettings& GetSettings() const;
    
    // ========== Stats ==========
    [[nodiscard]] NavMeshStats GetStats() const;
    [[nodiscard]] bool IsBuilt() const { return m_navMesh != nullptr; }
    
    // ========== Progress Callback ==========
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = std::move(callback); }
    
    // ========== Debug ==========
    void SetDebugDrawEnabled(bool enabled) { m_debugDrawEnabled = enabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const { return m_debugDrawEnabled; }
    void SetDebugDrawColor(const DirectX::XMFLOAT4& color) { m_debugDrawColor = color; }
    [[nodiscard]] const DirectX::XMFLOAT4& GetDebugDrawColor() const { return m_debugDrawColor; }
    
    void DebugDraw(::UnoEngine::DebugRenderer* debugRenderer);
    [[nodiscard]] const dtNavMesh* GetDetourNavMesh() const { return m_navMesh; }
    
private:
    NavMeshManager() = default;
    ~NavMeshManager();
    
    NavMeshManager(const NavMeshManager&) = delete;
    NavMeshManager& operator=(const NavMeshManager&) = delete;
    
    // Internal build steps
    bool CreateHeightfield(const std::vector<StaticGeometry>& geometry);
    bool RasterizeGeometry(const std::vector<StaticGeometry>& geometry);
    bool FilterWalkableAreas();
    bool BuildCompactHeightfield();
    bool ErodeWalkableArea();
    bool BuildDistanceField();
    bool BuildRegions();
    bool BuildContours();
    bool BuildPolygonMesh();
    bool BuildDetailMesh();
    bool BuildNavMeshData();
    
    void CleanupBuildData();
    
    // Query helpers
    unsigned int GetNearestPoly(const DirectX::XMFLOAT3& pos) const;
    
    void ReportProgress(float progress, const char* stage);

    // ========== Members ==========
    // Recast context & config
    std::unique_ptr<rcContext> m_context;
    std::unique_ptr<rcConfig> m_config;
    
    // Build intermediate data
    rcHeightfield* m_heightfield = nullptr;
    rcCompactHeightfield* m_compactHeightfield = nullptr;
    rcContourSet* m_contourSet = nullptr;
    rcPolyMesh* m_polyMesh = nullptr;
    rcPolyMeshDetail* m_detailMesh = nullptr;
    
    // Runtime data
    dtNavMesh* m_navMesh = nullptr;
    dtNavMeshQuery* m_navMeshQuery = nullptr;
    
    // Settings & Stats
    NavMeshBuildSettings m_settings;
    NavMeshStats m_stats;
    
    // Progress callback
    ProgressCallback m_progressCallback;
    
    // Bounding box for geometry
    DirectX::XMFLOAT3 m_boundsMin = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_boundsMax = {0.0f, 0.0f, 0.0f};
    
    // Debug drawing
    bool m_debugDrawEnabled = false;
    DirectX::XMFLOAT4 m_debugDrawColor = {0.0f, 0.8f, 0.4f, 0.8f}; // 緑色
};


} // namespace UnoEngine::Navigation
