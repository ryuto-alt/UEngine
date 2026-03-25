#include "pch.h"
#include "NavMeshManager.h"
#include "Engine/Rendering/DebugRenderer.h"
#include "Engine/Math/Vector.h"
#include "Engine/Core/Logger.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourCrowd.h>

#include <nlohmann/json.hpp>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace UnoEngine::Navigation {

// カスタムrcContext（ログ・タイマー用）
class NavMeshContext : public rcContext
{
public:
    NavMeshContext() : rcContext(true) {}
    
protected:
    void doLog(const rcLogCategory category, const char* msg, const int len) override
    {
        // OutputDebugStringで出力（必要に応じて調整）
#ifdef _WIN32
        OutputDebugStringA(msg);
        OutputDebugStringA("\n");
#endif
        (void)category;
        (void)len;
    }
};

// ========== Singleton ==========
NavMeshManager& NavMeshManager::Get()
{
    static NavMeshManager instance;
    return instance;
}

NavMeshManager::~NavMeshManager()
{
    Shutdown();
}

// ========== Lifecycle ==========
void NavMeshManager::Initialize()
{
    m_context = std::make_unique<NavMeshContext>();
}

void NavMeshManager::Shutdown()
{
    CleanupBuildData();
    
    if (m_crowd)
    {
        dtFreeCrowd(m_crowd);
        m_crowd = nullptr;
    }
    
    if (m_navMeshQuery)
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
    }
    
    if (m_navMesh)
    {
        dtFreeNavMesh(m_navMesh);
        m_navMesh = nullptr;
    }
    
    m_context.reset();
    m_config.reset();
}

void NavMeshManager::CleanupBuildData()
{
    if (m_heightfield)
    {
        rcFreeHeightField(m_heightfield);
        m_heightfield = nullptr;
    }
    if (m_compactHeightfield)
    {
        rcFreeCompactHeightfield(m_compactHeightfield);
        m_compactHeightfield = nullptr;
    }
    if (m_contourSet)
    {
        rcFreeContourSet(m_contourSet);
        m_contourSet = nullptr;
    }
    if (m_polyMesh)
    {
        rcFreePolyMesh(m_polyMesh);
        m_polyMesh = nullptr;
    }
    if (m_detailMesh)
    {
        rcFreePolyMeshDetail(m_detailMesh);
        m_detailMesh = nullptr;
    }
}

// ========== Building ==========
bool NavMeshManager::BuildNavMesh(const std::vector<StaticGeometry>& geometry,
                                   const NavMeshBuildSettings& settings)
{
    if (geometry.empty())
    {
        return false;
    }
    
    if (!settings.Validate())
    {
        return false;
    }
    
    if (!m_context)
    {
        Initialize();
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    m_settings = settings;
    
    // 既存データをクリア（Crowdも含む - NavMeshに依存するため）
    CleanupBuildData();
    if (m_crowd)
    {
        dtFreeCrowd(m_crowd);
        m_crowd = nullptr;
    }
    if (m_navMeshQuery)
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
    }
    if (m_navMesh)
    {
        dtFreeNavMesh(m_navMesh);
        m_navMesh = nullptr;
    }
    
    ReportProgress(0.0f, "Initializing...");
    
    // Step 1: Create heightfield
    if (!CreateHeightfield(geometry))
        return false;
    
    ReportProgress(0.1f, "Rasterizing geometry...");
    
    // Step 2: Rasterize geometry
    if (!RasterizeGeometry(geometry))
        return false;
    
    ReportProgress(0.2f, "Filtering walkable areas...");
    
    // Step 3: Filter walkable areas
    if (!FilterWalkableAreas())
        return false;
    
    ReportProgress(0.3f, "Building compact heightfield...");
    
    // Step 4: Build compact heightfield
    if (!BuildCompactHeightfield())
        return false;
    
    ReportProgress(0.4f, "Eroding walkable area...");
    
    // Step 5: Erode walkable area
    if (!ErodeWalkableArea())
        return false;
    
    ReportProgress(0.5f, "Building distance field...");
    
    // Step 6: Build distance field
    if (!BuildDistanceField())
        return false;
    
    ReportProgress(0.55f, "Building regions...");
    
    // Step 7: Build regions
    if (!BuildRegions())
        return false;
    
    ReportProgress(0.6f, "Building contours...");
    
    // Step 8: Build contours
    if (!BuildContours())
        return false;
    
    ReportProgress(0.7f, "Building polygon mesh...");
    
    // Step 9: Build polygon mesh
    if (!BuildPolygonMesh())
        return false;
    
    ReportProgress(0.8f, "Building detail mesh...");
    
    // Step 10: Build detail mesh
    if (!BuildDetailMesh())
        return false;
    
    ReportProgress(0.9f, "Building navigation mesh...");
    
    // Step 11: Build Detour nav mesh
    if (!BuildNavMeshData())
        return false;
    
    // 中間データをクリーンアップ
    CleanupBuildData();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeSeconds = std::chrono::duration<float>(endTime - startTime).count();
    
    ReportProgress(1.0f, "Complete");
    
    return true;
}

bool NavMeshManager::CreateHeightfield(const std::vector<StaticGeometry>& geometry)
{
    // ジオメトリのバウンディングボックスを計算
    m_boundsMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    m_boundsMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    
    for (const auto& geo : geometry)
    {
        for (const auto& v : geo.vertices)
        {
            float px = v.x + geo.position.x;
            float py = v.y + geo.position.y;
            float pz = v.z + geo.position.z;
            
            m_boundsMin.x = (std::min)(m_boundsMin.x, px);
            m_boundsMin.y = (std::min)(m_boundsMin.y, py);
            m_boundsMin.z = (std::min)(m_boundsMin.z, pz);
            
            m_boundsMax.x = (std::max)(m_boundsMax.x, px);
            m_boundsMax.y = (std::max)(m_boundsMax.y, py);
            m_boundsMax.z = (std::max)(m_boundsMax.z, pz);
        }
    }
    
    // rcConfigを設定
    m_config = std::make_unique<rcConfig>();
    std::memset(m_config.get(), 0, sizeof(rcConfig));
    
    m_config->cs = m_settings.cellSize;
    m_config->ch = m_settings.cellHeight;
    m_config->walkableSlopeAngle = m_settings.agentMaxSlope;
    m_config->walkableHeight = static_cast<int>(std::ceilf(m_settings.agentHeight / m_settings.cellHeight));
    m_config->walkableClimb = static_cast<int>(std::floorf(m_settings.agentMaxClimb / m_settings.cellHeight));
    m_config->walkableRadius = static_cast<int>(std::ceilf(m_settings.agentRadius / m_settings.cellSize));
    m_config->maxEdgeLen = static_cast<int>(m_settings.maxEdgeLength / m_settings.cellSize);
    m_config->maxSimplificationError = m_settings.maxSimplificationError;
    m_config->minRegionArea = m_settings.minRegionArea;
    m_config->mergeRegionArea = m_settings.mergeRegionArea;
    m_config->maxVertsPerPoly = m_settings.maxVertsPerPoly;
    m_config->detailSampleDist = m_settings.detailSampleDist < 0.9f ? 0.0f : m_settings.cellSize * m_settings.detailSampleDist;
    m_config->detailSampleMaxError = m_settings.cellHeight * m_settings.detailSampleMaxError;
    
    // Bounds
    rcVcopy(m_config->bmin, &m_boundsMin.x);
    rcVcopy(m_config->bmax, &m_boundsMax.x);
    
    // Grid size
    rcCalcGridSize(m_config->bmin, m_config->bmax, m_config->cs, &m_config->width, &m_config->height);
    
    // Heightfield生成
    m_heightfield = rcAllocHeightfield();
    if (!m_heightfield)
    {
        return false;
    }
    
    if (!rcCreateHeightfield(m_context.get(), *m_heightfield,
                             m_config->width, m_config->height,
                             m_config->bmin, m_config->bmax,
                             m_config->cs, m_config->ch))
    {
        return false;
    }
    
    return true;
}

bool NavMeshManager::RasterizeGeometry(const std::vector<StaticGeometry>& geometry)
{
    // 三角形をラスタライズ
    for (const auto& geo : geometry)
    {
        if (geo.vertices.empty() || geo.indices.empty())
            continue;
        
        // ワールド座標に変換した頂点配列を作成
        std::vector<float> verts;
        verts.reserve(geo.vertices.size() * 3);
        for (const auto& v : geo.vertices)
        {
            verts.push_back(v.x + geo.position.x);
            verts.push_back(v.y + geo.position.y);
            verts.push_back(v.z + geo.position.z);
        }
        
        const int numTris = static_cast<int>(geo.indices.size() / 3);
        
        // 歩行可能エリアのフラグ（全て歩行可能として設定）
        std::vector<unsigned char> areas(numTris, RC_WALKABLE_AREA);
        
        // 傾斜に基づいて歩行不可能エリアをマーク
        rcMarkWalkableTriangles(m_context.get(),
                                 m_config->walkableSlopeAngle,
                                 verts.data(),
                                 static_cast<int>(geo.vertices.size()),
                                 reinterpret_cast<const int*>(geo.indices.data()),
                                 numTris,
                                 areas.data());
        
        // 三角形をラスタライズ
        if (!rcRasterizeTriangles(m_context.get(),
                                   verts.data(),
                                   static_cast<int>(geo.vertices.size()),
                                   reinterpret_cast<const int*>(geo.indices.data()),
                                   areas.data(),
                                   numTris,
                                   *m_heightfield,
                                   m_config->walkableClimb))
        {
            return false;
        }
    }
    
    return true;
}

bool NavMeshManager::FilterWalkableAreas()
{
    if (m_settings.filterLowHangingObstacles)
    {
        rcFilterLowHangingWalkableObstacles(m_context.get(),
                                             m_config->walkableClimb,
                                             *m_heightfield);
    }
    
    if (m_settings.filterLedgeSpans)
    {
        rcFilterLedgeSpans(m_context.get(),
                           m_config->walkableHeight,
                           m_config->walkableClimb,
                           *m_heightfield);
    }
    
    if (m_settings.filterWalkableLowHeightSpans)
    {
        rcFilterWalkableLowHeightSpans(m_context.get(),
                                        m_config->walkableHeight,
                                        *m_heightfield);
    }
    
    return true;
}

bool NavMeshManager::BuildCompactHeightfield()
{
    m_compactHeightfield = rcAllocCompactHeightfield();
    if (!m_compactHeightfield)
    {
        return false;
    }
    
    if (!rcBuildCompactHeightfield(m_context.get(),
                                    m_config->walkableHeight,
                                    m_config->walkableClimb,
                                    *m_heightfield,
                                    *m_compactHeightfield))
    {
        return false;
    }
    
    // Heightfieldはもう不要
    rcFreeHeightField(m_heightfield);
    m_heightfield = nullptr;
    
    return true;
}

bool NavMeshManager::ErodeWalkableArea()
{
    if (!rcErodeWalkableArea(m_context.get(),
                              m_config->walkableRadius,
                              *m_compactHeightfield))
    {
        return false;
    }
    
    return true;
}

bool NavMeshManager::BuildDistanceField()
{
    if (!rcBuildDistanceField(m_context.get(), *m_compactHeightfield))
    {
        return false;
    }
    
    return true;
}

bool NavMeshManager::BuildRegions()
{
    if (m_settings.useMonotone)
    {
        // 単調分割（より安定）
        if (!rcBuildRegionsMonotone(m_context.get(),
                                     *m_compactHeightfield,
                                     0, // borderSize
                                     m_config->minRegionArea,
                                     m_config->mergeRegionArea))
        {
            return false;
        }
    }
    else
    {
        // Watershed分割
        if (!rcBuildRegions(m_context.get(),
                            *m_compactHeightfield,
                            0, // borderSize
                            m_config->minRegionArea,
                            m_config->mergeRegionArea))
        {
            return false;
        }
    }
    
    return true;
}

bool NavMeshManager::BuildContours()
{
    m_contourSet = rcAllocContourSet();
    if (!m_contourSet)
    {
        return false;
    }
    
    if (!rcBuildContours(m_context.get(),
                          *m_compactHeightfield,
                          m_config->maxSimplificationError,
                          m_config->maxEdgeLen,
                          *m_contourSet))
    {
        return false;
    }
    
    return true;
}

bool NavMeshManager::BuildPolygonMesh()
{
    m_polyMesh = rcAllocPolyMesh();
    if (!m_polyMesh)
    {
        return false;
    }
    
    if (!rcBuildPolyMesh(m_context.get(),
                          *m_contourSet,
                          m_config->maxVertsPerPoly,
                          *m_polyMesh))
    {
        return false;
    }
    
    return true;
}

bool NavMeshManager::BuildDetailMesh()
{
    m_detailMesh = rcAllocPolyMeshDetail();
    if (!m_detailMesh)
    {
        return false;
    }
    
    if (!rcBuildPolyMeshDetail(m_context.get(),
                                *m_polyMesh,
                                *m_compactHeightfield,
                                m_config->detailSampleDist,
                                m_config->detailSampleMaxError,
                                *m_detailMesh))
    {
        return false;
    }
    
    // CompactHeightfieldはもう不要
    rcFreeCompactHeightfield(m_compactHeightfield);
    m_compactHeightfield = nullptr;
    
    // ContourSetはもう不要
    rcFreeContourSet(m_contourSet);
    m_contourSet = nullptr;
    
    return true;
}

bool NavMeshManager::BuildNavMeshData()
{
    if (!m_polyMesh || m_polyMesh->npolys == 0)
    {
        return false;
    }
    
    // ポリゴンに歩行可能フラグを設定
    for (int i = 0; i < m_polyMesh->npolys; ++i)
    {
        if (m_polyMesh->areas[i] == RC_WALKABLE_AREA)
        {
            m_polyMesh->flags[i] = 1; // 歩行可能
        }
    }
    
    // Detour NavMesh作成パラメータ
    dtNavMeshCreateParams params;
    std::memset(&params, 0, sizeof(params));
    
    params.verts = m_polyMesh->verts;
    params.vertCount = m_polyMesh->nverts;
    params.polys = m_polyMesh->polys;
    params.polyAreas = m_polyMesh->areas;
    params.polyFlags = m_polyMesh->flags;
    params.polyCount = m_polyMesh->npolys;
    params.nvp = m_polyMesh->nvp;
    
    params.detailMeshes = m_detailMesh->meshes;
    params.detailVerts = m_detailMesh->verts;
    params.detailVertsCount = m_detailMesh->nverts;
    params.detailTris = m_detailMesh->tris;
    params.detailTriCount = m_detailMesh->ntris;
    
    params.walkableHeight = m_settings.agentHeight;
    params.walkableRadius = m_settings.agentRadius;
    params.walkableClimb = m_settings.agentMaxClimb;
    
    rcVcopy(params.bmin, m_polyMesh->bmin);
    rcVcopy(params.bmax, m_polyMesh->bmax);
    
    params.cs = m_config->cs;
    params.ch = m_config->ch;
    params.buildBvTree = true;
    
    // NavMeshデータ作成
    unsigned char* navData = nullptr;
    int navDataSize = 0;
    
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
    {
        return false;
    }
    
    // dtNavMesh作成
    m_navMesh = dtAllocNavMesh();
    if (!m_navMesh)
    {
        dtFree(navData);
        return false;
    }
    
    dtStatus status = m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
    if (dtStatusFailed(status))
    {
        dtFree(navData);
        dtFreeNavMesh(m_navMesh);
        m_navMesh = nullptr;
        return false;
    }
    
    // dtNavMeshQuery作成
    m_navMeshQuery = dtAllocNavMeshQuery();
    if (!m_navMeshQuery)
    {
        return false;
    }
    
    status = m_navMeshQuery->init(m_navMesh, 2048);
    if (dtStatusFailed(status))
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
        return false;
    }
    
    // 統計情報を更新
    m_stats.polyCount = m_polyMesh->npolys;
    m_stats.vertexCount = m_polyMesh->nverts;
    m_stats.memoryUsage = navDataSize;
    m_stats.tileCount = 1; // Single tile
    
    return true;
}

// ========== Queries ==========
bool NavMeshManager::FindPath(const DirectX::XMFLOAT3& start, 
                               const DirectX::XMFLOAT3& goal,
                               std::vector<DirectX::XMFLOAT3>& outPath)
{
    outPath.clear();
    
    if (!m_navMeshQuery)
    {
        return false;
    }
    
    const float polyPickExt[3] = { 2.0f, 4.0f, 2.0f };
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);
    
    // 開始点に最も近いポリゴンを検索
    dtPolyRef startRef = 0;
    float startNearest[3];
    dtStatus status = m_navMeshQuery->findNearestPoly(&start.x, polyPickExt, &filter, &startRef, startNearest);
    if (dtStatusFailed(status) || startRef == 0)
    {
        return false;
    }
    
    // 終了点に最も近いポリゴンを検索
    dtPolyRef endRef = 0;
    float endNearest[3];
    status = m_navMeshQuery->findNearestPoly(&goal.x, polyPickExt, &filter, &endRef, endNearest);
    if (dtStatusFailed(status) || endRef == 0)
    {
        return false;
    }
    
    // パス検索
    static constexpr int kMaxPathPolys = 256;
    dtPolyRef polys[kMaxPathPolys];
    int npolys = 0;

    status = m_navMeshQuery->findPath(startRef, endRef, startNearest, endNearest, &filter, polys, &npolys, kMaxPathPolys);
    if (dtStatusFailed(status) || npolys == 0)
    {
        return false;
    }

    // パスを直線パスに変換
    static constexpr int kMaxStraightPath = 256;
    float straightPath[kMaxStraightPath * 3];
    unsigned char straightPathFlags[kMaxStraightPath];
    dtPolyRef straightPathPolys[kMaxStraightPath];
    int nstraightPath = 0;

    status = m_navMeshQuery->findStraightPath(startNearest, endNearest,
                                               polys, npolys,
                                               straightPath, straightPathFlags, straightPathPolys,
                                               &nstraightPath, kMaxStraightPath);
    if (dtStatusFailed(status))
    {
        return false;
    }
    
    // 結果を出力
    outPath.reserve(nstraightPath);
    for (int i = 0; i < nstraightPath; ++i)
    {
        outPath.push_back({
            straightPath[i * 3 + 0],
            straightPath[i * 3 + 1],
            straightPath[i * 3 + 2]
        });
    }
    
    return true;
}

DirectX::XMFLOAT3 NavMeshManager::GetNextPathPoint(const DirectX::XMFLOAT3& currentPos, 
                                                    const DirectX::XMFLOAT3& goal,
                                                    float lookAheadDist) const
{
    if (!m_navMeshQuery)
    {
        return currentPos;
    }
    
    std::vector<DirectX::XMFLOAT3> path;
    
    // const_castでFindPathを呼び出し（本来はFindPathもconstにすべき）
    if (!const_cast<NavMeshManager*>(this)->FindPath(currentPos, goal, path))
    {
        return goal;
    }
    
    if (path.size() < 2)
    {
        return goal;
    }
    
    // lookAheadDistの距離にある点を探す
    float totalDist = 0.0f;
    for (size_t i = 1; i < path.size(); ++i)
    {
        float dx = path[i].x - path[i - 1].x;
        float dy = path[i].y - path[i - 1].y;
        float dz = path[i].z - path[i - 1].z;
        float segmentDist = std::sqrtf(dx * dx + dy * dy + dz * dz);
        
        if (totalDist + segmentDist >= lookAheadDist)
        {
            // この区間内に目標点がある
            float t = (lookAheadDist - totalDist) / segmentDist;
            return {
                path[i - 1].x + dx * t,
                path[i - 1].y + dy * t,
                path[i - 1].z + dz * t
            };
        }
        
        totalDist += segmentDist;
    }
    
    // パス全長がlookAheadDistより短い場合は最終点を返す
    return path.back();
}

bool NavMeshManager::IsPointOnNavMesh(const DirectX::XMFLOAT3& pos) const
{
    if (!m_navMeshQuery)
    {
        return false;
    }
    
    const float polyPickExt[3] = { 0.5f, 2.0f, 0.5f };
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);
    
    dtPolyRef ref = 0;
    float nearest[3];
    
    dtStatus status = m_navMeshQuery->findNearestPoly(&pos.x, polyPickExt, &filter, &ref, nearest);
    
    return dtStatusSucceed(status) && ref != 0;
}

unsigned int NavMeshManager::GetNearestPoly(const DirectX::XMFLOAT3& pos) const
{
    if (!m_navMeshQuery)
    {
        return 0;
    }
    
    const float polyPickExt[3] = { 2.0f, 4.0f, 2.0f };
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);
    
    dtPolyRef ref = 0;
    float nearest[3];
    
    m_navMeshQuery->findNearestPoly(&pos.x, polyPickExt, &filter, &ref, nearest);
    
    return ref;
}

// ========== I/O ==========
bool NavMeshManager::SaveNavMesh(const std::string& filename) const
{
    if (!m_navMesh)
    {
        return false;
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    
    // ヘッダー書き込み
    const char magic[] = "RNAV";
    file.write(magic, 4);
    
    // バージョン
    int version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // 設定を保存
    file.write(reinterpret_cast<const char*>(&m_settings), sizeof(m_settings));
    
    // タイルデータを保存
    const dtNavMesh* navMesh = m_navMesh;
    int numTiles = 0;
    
    for (int i = 0; i < navMesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = navMesh->getTile(i);
        if (!tile || !tile->header || !tile->dataSize) continue;
        numTiles++;
    }
    
    file.write(reinterpret_cast<const char*>(&numTiles), sizeof(numTiles));
    
    for (int i = 0; i < navMesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = navMesh->getTile(i);
        if (!tile || !tile->header || !tile->dataSize) continue;
        
        dtTileRef tileRef = navMesh->getTileRef(tile);
        file.write(reinterpret_cast<const char*>(&tileRef), sizeof(tileRef));
        file.write(reinterpret_cast<const char*>(&tile->dataSize), sizeof(tile->dataSize));
        file.write(reinterpret_cast<const char*>(tile->data), tile->dataSize);
    }
    
    return true;
}

bool NavMeshManager::LoadNavMesh(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    
    // マジック確認
    char magic[4];
    file.read(magic, 4);
    if (std::strncmp(magic, "RNAV", 4) != 0)
    {
        return false;
    }
    
    // バージョン確認
    int version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1)
    {
        return false;
    }
    
    // 既存データをクリア
    Shutdown();
    Initialize();
    
    // 設定を読み込み
    file.read(reinterpret_cast<char*>(&m_settings), sizeof(m_settings));
    
    // タイル数を読み込み
    int numTiles = 0;
    file.read(reinterpret_cast<char*>(&numTiles), sizeof(numTiles));
    
    // NavMesh作成
    m_navMesh = dtAllocNavMesh();
    if (!m_navMesh)
    {
        return false;
    }
    
    // シングルタイルNavMeshの場合は直接init(data, size, flags)を使用
    if (numTiles == 1)
    {
        dtTileRef tileRef = 0;
        int dataSize = 0;
        
        file.read(reinterpret_cast<char*>(&tileRef), sizeof(tileRef));
        file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
        
        unsigned char* data = static_cast<unsigned char*>(dtAlloc(dataSize, DT_ALLOC_PERM));
        if (!data)
        {
            dtFreeNavMesh(m_navMesh);
            m_navMesh = nullptr;
            return false;
        }
        
        file.read(reinterpret_cast<char*>(data), dataSize);
        
        dtStatus status = m_navMesh->init(data, dataSize, DT_TILE_FREE_DATA);
        if (dtStatusFailed(status))
        {
            dtFree(data);
            dtFreeNavMesh(m_navMesh);
            m_navMesh = nullptr;
            return false;
        }
    }
    else
    {
        // マルチタイルNavMeshの場合
        dtNavMeshParams params;
        std::memset(&params, 0, sizeof(params));
        params.tileWidth = m_settings.tileSize * m_settings.cellSize;
        params.tileHeight = m_settings.tileSize * m_settings.cellSize;
        params.maxTiles = m_settings.maxTiles;
        params.maxPolys = 1 << 14;
        
        // 最初のタイルからorigを取得するため、先読みする
        std::streampos tilesStart = file.tellg();
        dtTileRef firstTileRef = 0;
        int firstDataSize = 0;
        file.read(reinterpret_cast<char*>(&firstTileRef), sizeof(firstTileRef));
        file.read(reinterpret_cast<char*>(&firstDataSize), sizeof(firstDataSize));
        
        if (firstDataSize >= static_cast<int>(sizeof(dtMeshHeader)))
        {
            dtMeshHeader header;
            file.read(reinterpret_cast<char*>(&header), sizeof(header));
            rcVcopy(params.orig, header.bmin);
            file.seekg(tilesStart);
        }
        
        dtStatus status = m_navMesh->init(&params);
        if (dtStatusFailed(status))
        {
            dtFreeNavMesh(m_navMesh);
            m_navMesh = nullptr;
            return false;
        }
        
        // タイルを読み込み
        for (int i = 0; i < numTiles; ++i)
        {
            dtTileRef tileRef = 0;
            int dataSize = 0;
            
            file.read(reinterpret_cast<char*>(&tileRef), sizeof(tileRef));
            file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
            
            unsigned char* data = static_cast<unsigned char*>(dtAlloc(dataSize, DT_ALLOC_PERM));
            if (!data)
            {
                continue;
            }
            
            file.read(reinterpret_cast<char*>(data), dataSize);
            
            m_navMesh->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, nullptr);
        }
    }
    
    // NavMeshQuery作成
    m_navMeshQuery = dtAllocNavMeshQuery();
    if (!m_navMeshQuery)
    {
        return false;
    }
    
    dtStatus queryStatus = m_navMeshQuery->init(m_navMesh, 2048);
    if (dtStatusFailed(queryStatus))
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
        return false;
    }

    // NavMeshロード後にCrowdも初期化（エージェントが動作するために必要）
    InitializeCrowd(128, m_settings.agentRadius);

    return true;
}

// ========== Settings ==========
void NavMeshManager::SetSettings(const NavMeshBuildSettings& settings)
{
    m_settings = settings;
}

const NavMeshBuildSettings& NavMeshManager::GetSettings() const
{
    return m_settings;
}

// ========== Stats ==========
NavMeshStats NavMeshManager::GetStats() const
{
    return m_stats;
}

// ========== Debug ==========
void NavMeshManager::DebugDraw(::UnoEngine::DebugRenderer* debugRenderer)
{
    if (!debugRenderer || !m_navMesh || !m_debugDrawEnabled)
    {
        return;
    }
    
    // 半透明の塗りつぶし色
    const UnoEngine::Vector4 fillColor(m_debugDrawColor.x, m_debugDrawColor.y, m_debugDrawColor.z, 0.5f);
    // 境界エッジ色（黒）
    const UnoEngine::Vector4 edgeColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    const dtNavMesh* navMesh = m_navMesh;
    
    // すべてのタイルを走査
    for (int i = 0; i < navMesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = navMesh->getTile(i);
        if (!tile || !tile->header)
            continue;
        
        // タイル内のすべてのポリゴンを描画
        for (int j = 0; j < tile->header->polyCount; ++j)
        {
            const dtPoly* poly = &tile->polys[j];
            
            // オフメッシュコネクションはスキップ
            if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                continue;
            
            const dtPolyDetail* pd = &tile->detailMeshes[j];
            
            // ディテールメッシュの三角形を塗りつぶし描画
            for (unsigned int k = 0; k < pd->triCount; ++k)
            {
                const unsigned char* t = &tile->detailTris[(pd->triBase + k) * 4];
                UnoEngine::Vector3 triVerts[3];
                
                for (int l = 0; l < 3; ++l)
                {
                    if (t[l] < poly->vertCount)
                    {
                        const float* v = &tile->verts[poly->verts[t[l]] * 3];
                        // 少し上にオフセットしてZファイティング防止
                        triVerts[l] = UnoEngine::Vector3(v[0], v[1] + 0.02f, v[2]);
                    }
                    else
                    {
                        const float* v = &tile->detailVerts[(pd->vertBase + t[l] - poly->vertCount) * 3];
                        triVerts[l] = UnoEngine::Vector3(v[0], v[1] + 0.02f, v[2]);
                    }
                }
                
                // 三角形を塗りつぶし描画
                debugRenderer->AddTriangle(triVerts[0], triVerts[1], triVerts[2], fillColor);
            }
            
            // ポリゴンの外周エッジのみを描画（境界線）
            for (unsigned int k = 0; k < poly->vertCount; ++k)
            {
                // 隣接ポリゴンがない場合は境界エッジ
                if (poly->neis[k] == 0)
                {
                    const float* v0 = &tile->verts[poly->verts[k] * 3];
                    const float* v1 = &tile->verts[poly->verts[(k + 1) % poly->vertCount] * 3];
                    
                    UnoEngine::Vector3 start(v0[0], v0[1] + 0.05f, v0[2]);
                    UnoEngine::Vector3 end(v1[0], v1[1] + 0.05f, v1[2]);
                    
                    debugRenderer->AddLine(start, end, edgeColor);
                }
            }
        }
    }
}

void NavMeshManager::ReportProgress(float progress, const char* stage)
{
    if (m_progressCallback)
    {
        m_progressCallback(progress, stage);
    }
}

// ========== Crowd (Agent Management) ==========
bool NavMeshManager::InitializeCrowd(int maxAgents, float maxAgentRadius)
{
    if (!m_navMesh)
    {
        return false;
    }
    
    if (m_crowd)
    {
        dtFreeCrowd(m_crowd);
        m_crowd = nullptr;
    }
    
    m_crowd = dtAllocCrowd();
    if (!m_crowd)
    {
        return false;
    }
    
    if (!m_crowd->init(maxAgents, maxAgentRadius, m_navMesh))
    {
        dtFreeCrowd(m_crowd);
        m_crowd = nullptr;
        return false;
    }
    
    // 障害物回避パラメータをJSONから読み込み
    auto loadPreset = [](const nlohmann::json& preset, dtObstacleAvoidanceParams& out) {
        out.velBias       = preset.value("velBias", 0.4f);
        out.weightDesVel  = preset.value("weightDesVel", 2.0f);
        out.weightCurVel  = preset.value("weightCurVel", 0.75f);
        out.weightSide    = preset.value("weightSide", 0.75f);
        out.weightToi     = preset.value("weightToi", 2.5f);
        out.horizTime     = preset.value("horizTime", 2.5f);
        out.gridSize      = static_cast<unsigned char>(preset.value("gridSize", 33));
        out.adaptiveDivs  = static_cast<unsigned char>(preset.value("adaptiveDivs", 7));
        out.adaptiveRings = static_cast<unsigned char>(preset.value("adaptiveRings", 2));
        out.adaptiveDepth = static_cast<unsigned char>(preset.value("adaptiveDepth", 5));
    };

    nlohmann::json crowdConfig;
    {
        std::ifstream file("assets/config/crowd_avoidance.json");
        if (file.is_open()) {
            try { file >> crowdConfig; } catch (...) { crowdConfig = {}; }
        }
    }

    dtObstacleAvoidanceParams params;
    std::memset(&params, 0, sizeof(params));

    // プリセット0: 標準精度（狭い迷路向け）
    if (crowdConfig.contains("preset0"))
        loadPreset(crowdConfig["preset0"], params);
    else
        loadPreset({}, params);
    m_crowd->setObstacleAvoidanceParams(0, &params);

    // プリセット1: 高精度
    if (crowdConfig.contains("preset1"))
        loadPreset(crowdConfig["preset1"], params);
    else {
        params.adaptiveDivs  = 8;
        params.adaptiveRings = 3;
        params.adaptiveDepth = 6;
    }
    m_crowd->setObstacleAvoidanceParams(1, &params);

    return true;
}

int NavMeshManager::AddCrowdAgent(const DirectX::XMFLOAT3& position, float radius, float height,
                                   float maxSpeed, float maxAcceleration)
{
    if (!m_crowd)
    {
        return -1;
    }

    // エージェントパラメータ乗数をJSONから取得（crowd_avoidance.json → agentDefaults）
    static const auto agentDefaults = []() {
        struct AgentMultipliers {
            float acceleration = 10.0f;
            float collisionQueryRange = 12.0f;
            float pathOptimizationRange = 30.0f;
            float separationWeight = 0.0f;
        } defaults;
        std::ifstream file("assets/config/crowd_avoidance.json");
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;
                if (j.contains("agentDefaults")) {
                    auto& d = j["agentDefaults"];
                    defaults.acceleration            = d.value("accelerationMultiplier", defaults.acceleration);
                    defaults.collisionQueryRange     = d.value("collisionQueryRangeMultiplier", defaults.collisionQueryRange);
                    defaults.pathOptimizationRange   = d.value("pathOptimizationRangeMultiplier", defaults.pathOptimizationRange);
                    defaults.separationWeight        = d.value("separationWeight", defaults.separationWeight);
                }
            } catch (...) {}
        }
        return defaults;
    }();

    dtCrowdAgentParams ap;
    std::memset(&ap, 0, sizeof(ap));

    ap.radius = radius;
    ap.height = height;
    ap.maxAcceleration = maxAcceleration * agentDefaults.acceleration;
    ap.maxSpeed = maxSpeed;

    // NavMesh境界を考慮した回避範囲
    ap.collisionQueryRange = radius * agentDefaults.collisionQueryRange;
    ap.pathOptimizationRange = radius * agentDefaults.pathOptimizationRange;
    ap.separationWeight = agentDefaults.separationWeight;

    ap.updateFlags = DT_CROWD_ANTICIPATE_TURNS |
                     DT_CROWD_OBSTACLE_AVOIDANCE |
                     DT_CROWD_OPTIMIZE_VIS |
                     DT_CROWD_OPTIMIZE_TOPO;

    ap.obstacleAvoidanceType = 1;
    ap.queryFilterType = 0;
    ap.userData = nullptr;

    float pos[3] = { position.x, position.y, position.z };
    return m_crowd->addAgent(pos, &ap);
}

void NavMeshManager::RemoveCrowdAgent(int agentIndex)
{
    if (m_crowd && agentIndex >= 0)
    {
        m_crowd->removeAgent(agentIndex);
    }
}

void NavMeshManager::UpdateAgentParameters(int agentIndex, float maxSpeed, float maxAcceleration)
{
    if (!m_crowd || agentIndex < 0)
    {
        return;
    }
    
    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return;
    }
    
    dtCrowdAgentParams params = agent->params;
    params.maxSpeed = maxSpeed;
    params.maxAcceleration = maxAcceleration;
    m_crowd->updateAgentParameters(agentIndex, &params);
}

bool NavMeshManager::SetAgentTarget(int agentIndex, const DirectX::XMFLOAT3& target)
{
    if (!m_crowd || !m_navMeshQuery || agentIndex < 0)
    {
        return false;
    }
    
    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return false;
    }
    
    const float polyPickExt[3] = { 2.0f, 4.0f, 2.0f };
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);
    
    dtPolyRef targetRef = 0;
    float targetPos[3];
    float pos[3] = { target.x, target.y, target.z };
    
    dtStatus status = m_navMeshQuery->findNearestPoly(pos, polyPickExt, &filter, &targetRef, targetPos);
    if (dtStatusFailed(status) || targetRef == 0)
    {
        return false;
    }
    
    return m_crowd->requestMoveTarget(agentIndex, targetRef, targetPos);
}

void NavMeshManager::StopAgent(int agentIndex)
{
    if (m_crowd && agentIndex >= 0)
    {
        m_crowd->resetMoveTarget(agentIndex);
    }
}

void NavMeshManager::UpdateCrowd(float deltaTime)
{
    if (m_crowd)
    {
        constexpr float kMaxStep = 1.0f / 30.0f;
        float remaining = deltaTime;
        while (remaining > 0.0f)
        {
            float step = (remaining > kMaxStep) ? kMaxStep : remaining;
            m_crowd->update(step, nullptr);
            remaining -= step;
        }
    }
}

DirectX::XMFLOAT3 NavMeshManager::GetAgentPosition(int agentIndex) const
{
    if (!m_crowd || agentIndex < 0)
    {
        return { 0.0f, 0.0f, 0.0f };
    }
    
    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return { 0.0f, 0.0f, 0.0f };
    }
    
    return { agent->npos[0], agent->npos[1], agent->npos[2] };
}

DirectX::XMFLOAT3 NavMeshManager::GetAgentVelocity(int agentIndex) const
{
    if (!m_crowd || agentIndex < 0)
    {
        return { 0.0f, 0.0f, 0.0f };
    }
    
    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return { 0.0f, 0.0f, 0.0f };
    }
    
    return { agent->vel[0], agent->vel[1], agent->vel[2] };
}

bool NavMeshManager::IsAgentActive(int agentIndex) const
{
    if (!m_crowd || agentIndex < 0)
    {
        return false;
    }
    
    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    return agent && agent->active;
}

bool NavMeshManager::HasAgentReachedTarget(int agentIndex, float tolerance) const
{
    if (!m_crowd || agentIndex < 0)
    {
        return false;
    }
    
    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return false;
    }
    
    // ターゲットがない場合は到達とみなす
    if (agent->targetState == DT_CROWDAGENT_TARGET_NONE ||
        agent->targetState == DT_CROWDAGENT_TARGET_FAILED)
    {
        return true;
    }
    
    // 目的地との距離を計算
    float dx = agent->targetPos[0] - agent->npos[0];
    float dy = agent->targetPos[1] - agent->npos[1];
    float dz = agent->targetPos[2] - agent->npos[2];
    float distSq = dx * dx + dy * dy + dz * dz;
    
    return distSq < tolerance * tolerance;
}

bool NavMeshManager::GetCrowdAgentDebugInfo(int agentIndex, CrowdAgentDebugInfo& outInfo) const
{
    if (!m_crowd || agentIndex < 0)
    {
        return false;
    }

    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return false;
    }

    outInfo.active = agent->active;
    outInfo.state = agent->state;
    outInfo.partial = agent->partial;
    outInfo.ncorners = agent->ncorners;
    outInfo.targetState = agent->targetState;
    outInfo.targetReplan = agent->targetReplan;
    outInfo.targetReplanTime = agent->targetReplanTime;
    outInfo.desiredSpeed = agent->desiredSpeed;
    outInfo.dvel = { agent->dvel[0], agent->dvel[1], agent->dvel[2] };
    outInfo.nvel = { agent->nvel[0], agent->nvel[1], agent->nvel[2] };
    outInfo.targetPos = { agent->targetPos[0], agent->targetPos[1], agent->targetPos[2] };

    return true;
}

bool NavMeshManager::GetRandomPointOnNavMesh(DirectX::XMFLOAT3& outPoint) const
{
    if (!m_navMeshQuery)
    {
        Logger::Warning("[NavMesh] GetRandomPointOnNavMesh failed: m_navMeshQuery is null");
        return false;
    }
    
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);
    
    dtPolyRef randomRef = 0;
    float randomPt[3];
    
    // ランダムシードを生成
    auto seed = static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
    srand(seed);
    
    auto frand = []() -> float {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    };
    
    dtStatus status = m_navMeshQuery->findRandomPoint(&filter, frand, &randomRef, randomPt);
    if (dtStatusFailed(status))
    {
        Logger::Warning("[NavMesh] findRandomPoint failed with status: 0x{:X}", status);
        return false;
    }
    
    outPoint = { randomPt[0], randomPt[1], randomPt[2] };
    return true;
}

bool NavMeshManager::GetRandomPointAroundCircle(const DirectX::XMFLOAT3& center,
                                                 float radius,
                                                 DirectX::XMFLOAT3& outPoint) const
{
    if (!m_navMeshQuery)
    {
        return false;
    }

    const float polyPickExt[3] = { 2.0f, 4.0f, 2.0f };
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);

    dtPolyRef centerRef = 0;
    float centerPos[3];
    float pos[3] = { center.x, center.y, center.z };

    dtStatus status = m_navMeshQuery->findNearestPoly(pos, polyPickExt, &filter, &centerRef, centerPos);
    if (dtStatusFailed(status) || centerRef == 0)
    {
        Logger::Warning("[NavMesh] findNearestPoly failed at ({:.2f}, {:.2f}, {:.2f}) status=0x{:X} ref={}",
            pos[0], pos[1], pos[2], status, centerRef);
        return false;
    }

    auto seed = static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
    srand(seed);

    auto frand = []() -> float {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    };

    dtPolyRef randomRef = 0;
    float randomPt[3];

    status = m_navMeshQuery->findRandomPointAroundCircle(centerRef, centerPos, radius, &filter, frand, &randomRef, randomPt);
    if (dtStatusFailed(status))
    {
        return false;
    }

    outPoint = { randomPt[0], randomPt[1], randomPt[2] };
    return true;
}

bool NavMeshManager::GetNextCorner(int agentIndex, DirectX::XMFLOAT3& outCorner, float& outDistToCorner) const
{
    if (!m_crowd || agentIndex < 0)
    {
        return false;
    }

    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return false;
    }

    static constexpr float kMinLookaheadDist = 0.3f;

    // コーナーリストから十分遠いものを探す
    for (int i = 0; i < agent->ncorners; ++i)
    {
        float cx = agent->cornerVerts[i * 3 + 0];
        float cy = agent->cornerVerts[i * 3 + 1];
        float cz = agent->cornerVerts[i * 3 + 2];

        float dx = cx - agent->npos[0];
        float dy = cy - agent->npos[1];
        float dz = cz - agent->npos[2];
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist >= kMinLookaheadDist)
        {
            outCorner.x = cx;
            outCorner.y = cy;
            outCorner.z = cz;
            outDistToCorner = dist;
            return true;
        }
    }

    // 十分遠いコーナーがない場合は目標位置を使用
    if (agent->targetState == DT_CROWDAGENT_TARGET_VALID ||
        agent->targetState == DT_CROWDAGENT_TARGET_VELOCITY)
    {
        outCorner.x = agent->targetPos[0];
        outCorner.y = agent->targetPos[1];
        outCorner.z = agent->targetPos[2];

        float dx = outCorner.x - agent->npos[0];
        float dy = outCorner.y - agent->npos[1];
        float dz = outCorner.z - agent->npos[2];
        outDistToCorner = std::sqrt(dx * dx + dy * dy + dz * dz);
        return true;
    }

    return false;
}

void NavMeshManager::OverrideAgentVelocity(int agentIndex, const DirectX::XMFLOAT3& velocity)
{
    if (!m_crowd || agentIndex < 0)
    {
        return;
    }

    const dtCrowdAgent* agent = m_crowd->getAgent(agentIndex);
    if (!agent || !agent->active)
    {
        return;
    }

    float vel[3] = { velocity.x, velocity.y, velocity.z };
    m_crowd->requestMoveVelocity(agentIndex, vel);
}

bool NavMeshManager::GetNavMeshCenter(DirectX::XMFLOAT3& outCenter) const
{
    if (!m_navMesh)
    {
        return false;
    }

    // バウンディングボックスの中心を計算
    outCenter.x = (m_boundsMin.x + m_boundsMax.x) * 0.5f;
    outCenter.y = (m_boundsMin.y + m_boundsMax.y) * 0.5f;
    outCenter.z = (m_boundsMin.z + m_boundsMax.z) * 0.5f;

    // 中心点がNavMesh上にあるか確認し、なければ最近接点を探す
    const float polyPickExt[3] = { 10.0f, 10.0f, 10.0f };
    dtQueryFilter filter;
    filter.setIncludeFlags(0xFFFF);
    filter.setExcludeFlags(0);

    dtPolyRef nearestRef = 0;
    float nearestPt[3];
    float pos[3] = { outCenter.x, outCenter.y, outCenter.z };

    if (m_navMeshQuery)
    {
        dtStatus status = m_navMeshQuery->findNearestPoly(pos, polyPickExt, &filter, &nearestRef, nearestPt);
        if (dtStatusSucceed(status) && nearestRef != 0)
        {
            outCenter.x = nearestPt[0];
            outCenter.y = nearestPt[1];
            outCenter.z = nearestPt[2];
            return true;
        }
    }

    return true;
}

bool NavMeshManager::GetNavMeshBounds(DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax) const
{
    if (!m_navMesh)
    {
        return false;
    }

    outMin = m_boundsMin;
    outMax = m_boundsMax;
    return true;
}

} // namespace UnoEngine::Navigation
