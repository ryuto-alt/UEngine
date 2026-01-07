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
class dtCrowd;
struct dtCrowdAgentParams;

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
    
    // ========== Crowd (Agent Management) ==========
    /// Crowdシステムを初期化（NavMeshビルド後に呼び出し）
    bool InitializeCrowd(int maxAgents = 128, float maxAgentRadius = 0.6f);
    
    /// エージェントを追加し、インデックスを返す（-1は失敗）
    int AddCrowdAgent(const DirectX::XMFLOAT3& position, float radius, float height,
                      float maxSpeed = 3.5f, float maxAcceleration = 8.0f);
    
    /// エージェントを削除
    void RemoveCrowdAgent(int agentIndex);
    
    /// エージェントの目的地を設定
    bool SetAgentTarget(int agentIndex, const DirectX::XMFLOAT3& target);
    
    /// エージェントの移動を停止
    void StopAgent(int agentIndex);
    
    /// Crowdを更新（毎フレーム呼び出し）
    void UpdateCrowd(float deltaTime);
    
    /// エージェント情報取得
    [[nodiscard]] DirectX::XMFLOAT3 GetAgentPosition(int agentIndex) const;
    [[nodiscard]] DirectX::XMFLOAT3 GetAgentVelocity(int agentIndex) const;
    [[nodiscard]] bool IsAgentActive(int agentIndex) const;
    [[nodiscard]] bool HasAgentReachedTarget(int agentIndex, float tolerance = 0.5f) const;
    
    /// ランダムなNavMesh上の点を取得（徘徊用）
    [[nodiscard]] bool GetRandomPointOnNavMesh(DirectX::XMFLOAT3& outPoint) const;
    
    /// 指定点の周囲のランダムなNavMesh上の点を取得
    [[nodiscard]] bool GetRandomPointAroundCircle(const DirectX::XMFLOAT3& center, 
                                                   float radius,
                                                   DirectX::XMFLOAT3& outPoint) const;
    
    /// Crowdが初期化済みかどうか
    [[nodiscard]] bool IsCrowdInitialized() const { return m_crowd != nullptr; }
    
    // ========== Build State ==========
    /// ビルド中かどうかを設定（非同期ビルド用）
    void SetBuilding(bool building) { m_isBuilding = building; }
    /// ビルド中かどうかを取得
    [[nodiscard]] bool IsBuilding() const { return m_isBuilding; }
    
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
    dtCrowd* m_crowd = nullptr;
    
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
    
    // Build state
    bool m_isBuilding = false;
};


} // namespace UnoEngine::Navigation
