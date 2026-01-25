#include "pch.h"
#include "NavMeshBuilder.h"
#include "../../Core/Scene.h"
#include "../../Core/GameObject.h"
#include "../../Core/CollisionComponent.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <queue>

namespace UnoEngine
{

// NavMeshInputGeometry実装
void NavMeshInputGeometry::AddTriangle(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2)
{
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    indices.push_back(baseIndex);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
}

void NavMeshInputGeometry::AddAABB(const AABB& aabb)
{
    // AABBから上面の4頂点を取得
    float minX = aabb.min.GetX(), maxX = aabb.max.GetX();
    float maxY = aabb.max.GetY();
    float minZ = aabb.min.GetZ(), maxZ = aabb.max.GetZ();
    
    DirectX::XMFLOAT3 topCorners[4] = {
        {minX, maxY, minZ},
        {maxX, maxY, minZ},
        {maxX, maxY, maxZ},
        {minX, maxY, maxZ}
    };
    
    // 上面（Y+方向）を歩行可能面として追加
    AddTriangle(topCorners[0], topCorners[1], topCorners[2]);
    AddTriangle(topCorners[0], topCorners[2], topCorners[3]);
}


// NavMeshBuilder実装
std::unique_ptr<NavMeshData> NavMeshBuilder::Build(Scene* scene, const NavMeshConfig& config)
{
    NavMeshInputGeometry geometry;
    CollectGeometryFromScene(scene, geometry);
    
    if (geometry.IsEmpty())
        return nullptr;
    
    return Build(geometry, config);
}

std::unique_ptr<NavMeshData> NavMeshBuilder::Build(const NavMeshInputGeometry& geometry, const NavMeshConfig& config)
{
    if (geometry.IsEmpty())
        return nullptr;
    
    char debugBuf[256];
    snprintf(debugBuf, sizeof(debugBuf), "Geometry: %zu verts, %zu tris",
             geometry.vertices.size(), geometry.indices.size() / 3);
    ReportProgress(0.0f, debugBuf);
    
    // 1. ボクセル化
    HeightField heightField;
    ReportProgress(0.1f, "Voxelizing geometry...");
    if (!Voxelize(geometry, config, heightField))
    {
        ReportProgress(0.1f, "Voxelize failed");
        return nullptr;
    }
    
    // スパン数をカウント
    int totalSpans = 0;
    int walkableSpans = 0;
    for (const auto& spanPtr : heightField.spans)
    {
        for (auto* span = spanPtr.get(); span; span = span->next.get())
        {
            ++totalSpans;
            if (span->area > 0) ++walkableSpans;
        }
    }
    
    snprintf(debugBuf, sizeof(debugBuf), "Voxelized: %dx%d grid, %d spans, %d walkable",
             heightField.width, heightField.height, totalSpans, walkableSpans);
    ReportProgress(0.15f, debugBuf);
    
    // 2. 歩行可能領域のフィルタリング
    ReportProgress(0.2f, "Filtering walkable areas...");
    int walkableHeight = static_cast<int>(std::ceil(config.agentHeight / config.cellHeight));
    int walkableClimb = static_cast<int>(std::floor(config.stepHeight / config.cellHeight));
    FilterWalkableLowHeightSpans(heightField, static_cast<float>(walkableHeight));
    FilterLedgeSpans(heightField, static_cast<float>(walkableClimb));

    // フィルタリング後のスパン数
    walkableSpans = 0;
    for (const auto& spanPtr : heightField.spans)
    {
        for (auto* span = spanPtr.get(); span; span = span->next.get())
        {
            if (span->area > 0) ++walkableSpans;
        }
    }
    snprintf(debugBuf, sizeof(debugBuf), "After filter: %d walkable spans (minRegionArea=%.1f)",
             walkableSpans, config.minRegionArea);
    ReportProgress(0.35f, debugBuf);
    
    // 3. 領域分割
    std::vector<Region> regions;
    ReportProgress(0.4f, "Building regions...");
    if (!BuildRegions(heightField, config, regions))
    {
        snprintf(debugBuf, sizeof(debugBuf), "No regions found (need >= %.1f cells)", config.minRegionArea);
        ReportProgress(0.4f, debugBuf);
        return nullptr;
    }
    
    // 4. 輪郭抽出
    std::vector<Contour> contours;
    ReportProgress(0.6f, "Building contours...");
    if (!BuildContours(heightField, regions, config, contours))
        return nullptr;
    
    // 5. ポリゴン生成
    auto navMesh = std::make_unique<NavMeshData>();
    navMesh->config = config;
    ReportProgress(0.8f, "Building polygons...");
    if (!BuildPolygons(contours, config, *navMesh))
        return nullptr;
    
    // 6. 隣接情報構築
    ReportProgress(0.9f, "Building neighbor connections...");
    BuildNeighborConnections(*navMesh);
    
    // バウンディングボックス計算
    if (!navMesh->vertices.empty())
    {
        DirectX::XMFLOAT3 minPt = navMesh->vertices[0];
        DirectX::XMFLOAT3 maxPt = navMesh->vertices[0];
        for (const auto& v : navMesh->vertices)
        {
            minPt.x = std::min(minPt.x, v.x);
            minPt.y = std::min(minPt.y, v.y);
            minPt.z = std::min(minPt.z, v.z);
            maxPt.x = std::max(maxPt.x, v.x);
            maxPt.y = std::max(maxPt.y, v.y);
            maxPt.z = std::max(maxPt.z, v.z);
        }
        navMesh->bounds.Center = DirectX::XMFLOAT3(
            (minPt.x + maxPt.x) * 0.5f,
            (minPt.y + maxPt.y) * 0.5f,
            (minPt.z + maxPt.z) * 0.5f
        );
        navMesh->bounds.Extents = DirectX::XMFLOAT3(
            (maxPt.x - minPt.x) * 0.5f,
            (maxPt.y - minPt.y) * 0.5f,
            (maxPt.z - minPt.z) * 0.5f
        );
    }
    
    ReportProgress(1.0f, "NavMesh build complete!");
    return navMesh;
}

void NavMeshBuilder::CollectGeometryFromScene(Scene* scene, NavMeshInputGeometry& outGeometry)
{
    if (!scene) return;
    
    int totalObjects = 0;
    int collidersFound = 0;
    int walkableColliders = 0;
    
    for (const auto& gameObject : scene->GetGameObjects())
    {
        ++totalObjects;
        auto* collider = gameObject->GetComponent<CollisionComponent>();
        if (collider)
        {
            ++collidersFound;
            // NavMeshAreaがWalkableに設定されているコライダーのみ使用
            if (collider->IsEnabled() && collider->IsNavMeshWalkable())
            {
                ++walkableColliders;
                CollectGeometryFromCollider(collider, outGeometry);
            }
        }
    }
    
    // デバッグ情報をプログレスコールバックで報告
    char debugMsg[256];
    snprintf(debugMsg, sizeof(debugMsg), "Found %d objects, %d colliders (%d walkable), %zu triangles", 
             totalObjects, collidersFound, walkableColliders, outGeometry.indices.size() / 3);
    ReportProgress(0.05f, debugMsg);
}

void NavMeshBuilder::CollectGeometryFromCollider(CollisionComponent* collider, NavMeshInputGeometry& outGeometry)
{
    if (!collider) return;
    
    if (collider->HasMultipleAABBs())
    {
        for (const auto& aabb : collider->GetWorldAABBs())
        {
            outGeometry.AddAABB(aabb);
        }
    }
    else
    {
        outGeometry.AddAABB(collider->GetWorldAABB());
    }
}

bool NavMeshBuilder::Voxelize(const NavMeshInputGeometry& geometry, const NavMeshConfig& config, HeightField& outHeightField)
{
    // ジオメトリのバウンディングボックスを計算
    DirectX::XMFLOAT3 minPt = geometry.vertices[0];
    DirectX::XMFLOAT3 maxPt = geometry.vertices[0];
    for (const auto& v : geometry.vertices)
    {
        minPt.x = std::min(minPt.x, v.x);
        minPt.y = std::min(minPt.y, v.y);
        minPt.z = std::min(minPt.z, v.z);
        maxPt.x = std::max(maxPt.x, v.x);
        maxPt.y = std::max(maxPt.y, v.y);
        maxPt.z = std::max(maxPt.z, v.z);
    }
    
    // エージェント半径分だけ拡張
    minPt.x -= config.agentRadius;
    minPt.z -= config.agentRadius;
    maxPt.x += config.agentRadius;
    maxPt.z += config.agentRadius;
    
    // ハイトフィールドのサイズ計算
    outHeightField.width = static_cast<int>(std::ceil((maxPt.x - minPt.x) / config.cellSize));
    outHeightField.height = static_cast<int>(std::ceil((maxPt.z - minPt.z) / config.cellSize));
    outHeightField.origin = minPt;
    outHeightField.cellSize = config.cellSize;
    outHeightField.cellHeight = config.cellHeight;
    
    if (outHeightField.width <= 0 || outHeightField.height <= 0)
        return false;
    
    outHeightField.spans.resize(outHeightField.width * outHeightField.height);
    
    // 三角形をボクセル化
    for (size_t i = 0; i < geometry.indices.size(); i += 3)
    {
        const auto& v0 = geometry.vertices[geometry.indices[i]];
        const auto& v1 = geometry.vertices[geometry.indices[i + 1]];
        const auto& v2 = geometry.vertices[geometry.indices[i + 2]];
        
        // 三角形のバウンディングボックス
        float triMinX = std::min({v0.x, v1.x, v2.x});
        float triMaxX = std::max({v0.x, v1.x, v2.x});
        float triMinZ = std::min({v0.z, v1.z, v2.z});
        float triMaxZ = std::max({v0.z, v1.z, v2.z});
        float triMinY = std::min({v0.y, v1.y, v2.y});
        float triMaxY = std::max({v0.y, v1.y, v2.y});
        
        // セル範囲
        int x0 = std::max(0, static_cast<int>((triMinX - minPt.x) / config.cellSize));
        int x1 = std::min(outHeightField.width - 1, static_cast<int>((triMaxX - minPt.x) / config.cellSize));
        int z0 = std::max(0, static_cast<int>((triMinZ - minPt.z) / config.cellSize));
        int z1 = std::min(outHeightField.height - 1, static_cast<int>((triMaxZ - minPt.z) / config.cellSize));
        
        int minYCell = static_cast<int>((triMinY - minPt.y) / config.cellHeight);
        int maxYCell = static_cast<int>((triMaxY - minPt.y) / config.cellHeight);
        
        // 影響を受けるセルにスパンを追加
        for (int z = z0; z <= z1; ++z)
        {
            for (int x = x0; x <= x1; ++x)
            {
                int idx = z * outHeightField.width + x;

                auto span = std::make_unique<HeightSpan>();
                span->minY = minYCell;
                span->maxY = maxYCell;
                span->area = 1;
                span->next = std::move(outHeightField.spans[idx]);
                outHeightField.spans[idx] = std::move(span);
            }
        }
    }
    
    return true;
}

void NavMeshBuilder::FilterWalkableLowHeightSpans(HeightField& heightField, float walkableHeight)
{
    int walkableHeightCells = static_cast<int>(walkableHeight);

    for (int z = 0; z < heightField.height; ++z)
    {
        for (int x = 0; x < heightField.width; ++x)
        {
            int idx = z * heightField.width + x;
            for (auto* span = heightField.spans[idx].get(); span; span = span->next.get())
            {
                if (span->next)
                {
                    int gap = span->next->minY - span->maxY;
                    if (gap < walkableHeightCells)
                    {
                        span->area = 0;
                    }
                }
            }
        }
    }
}

void NavMeshBuilder::FilterLedgeSpans(HeightField& heightField, float walkableClimb)
{
    int maxClimbCells = static_cast<int>(walkableClimb);
    constexpr int dx[] = {-1, 0, 1, 0};
    constexpr int dz[] = {0, -1, 0, 1};

    for (int z = 0; z < heightField.height; ++z)
    {
        for (int x = 0; x < heightField.width; ++x)
        {
            int idx = z * heightField.width + x;
            for (auto* span = heightField.spans[idx].get(); span; span = span->next.get())
            {
                if (span->area == 0)
                    continue;

                int invalidNeighbors = 0;
                for (int dir = 0; dir < 4; ++dir)
                {
                    int nx = x + dx[dir];
                    int nz = z + dz[dir];

                    if (nx < 0 || nx >= heightField.width || nz < 0 || nz >= heightField.height)
                        continue;

                    int nidx = nz * heightField.width + nx;

                    bool hasValidNeighbor = false;
                    for (auto* neighborSpan = heightField.spans[nidx].get(); neighborSpan; neighborSpan = neighborSpan->next.get())
                    {
                        if (neighborSpan->area != 0)
                        {
                            int heightDiff = std::abs(span->maxY - neighborSpan->maxY);
                            if (heightDiff <= maxClimbCells)
                            {
                                hasValidNeighbor = true;
                                break;
                            }
                        }
                    }

                    if (!hasValidNeighbor)
                        ++invalidNeighbors;
                }

                if (invalidNeighbors >= 4)
                    span->area = 0;
            }
        }
    }
}

bool NavMeshBuilder::BuildRegions(HeightField& heightField, const NavMeshConfig& config, std::vector<Region>& outRegions)
{
    // 単純なフラッドフィル領域分割
    std::vector<uint32_t> regionIds(heightField.width * heightField.height, 0);
    uint32_t nextRegionId = 1;
    
    int walkableCellCount = 0;
    for (int i = 0; i < heightField.width * heightField.height; ++i)
    {
        if (heightField.spans[i] && heightField.spans[i]->area > 0)
            ++walkableCellCount;
    }
    char debugBuf[256];
    snprintf(debugBuf, sizeof(debugBuf), "BuildRegions: grid=%dx%d, walkable cells=%d", 
             heightField.width, heightField.height, walkableCellCount);
    OutputDebugStringA(debugBuf);
    OutputDebugStringA("\n");
    
    constexpr int dx[] = {-1, 0, 1, 0};
    constexpr int dz[] = {0, -1, 0, 1};

    for (int z = 0; z < heightField.height; ++z)
    {
        for (int x = 0; x < heightField.width; ++x)
        {
            int idx = z * heightField.width + x;
            auto* span = heightField.spans[idx].get();

            if (!span || span->area == 0 || regionIds[idx] != 0)
                continue;
            
            // フラッドフィルで領域を作成
            Region region;
            region.id = nextRegionId;
            region.minX = x;
            region.maxX = x;
            region.minZ = z;
            region.maxZ = z;
            
            std::queue<std::pair<int, int>> queue;
            queue.push({x, z});
            regionIds[idx] = nextRegionId;
            
            float heightSum = 0.0f;
            int cellCount = 0;
            
            while (!queue.empty())
            {
                auto [cx, cz] = queue.front();
                queue.pop();

                int cidx = cz * heightField.width + cx;
                auto* currentSpan = heightField.spans[cidx].get();
                
                region.cells.push_back({cx, cz});
                region.minX = std::min(region.minX, cx);
                region.maxX = std::max(region.maxX, cx);
                region.minZ = std::min(region.minZ, cz);
                region.maxZ = std::max(region.maxZ, cz);
                
                float cellY = heightField.origin.y + currentSpan->maxY * heightField.cellHeight;
                heightSum += cellY;
                cellCount++;
                
                // 隣接セルを探索
                for (int dir = 0; dir < 4; ++dir)
                {
                    int nx = cx + dx[dir];
                    int nz = cz + dz[dir];
                    
                    if (nx < 0 || nx >= heightField.width || nz < 0 || nz >= heightField.height)
                        continue;
                    
                    int nidx = nz * heightField.width + nx;
                    if (regionIds[nidx] != 0)
                        continue;

                    auto* neighborSpan = heightField.spans[nidx].get();
                    if (!neighborSpan || neighborSpan->area == 0)
                        continue;
                    
                    // 高さが近いスパンのみ同じ領域に含める
                    int heightDiff = std::abs(currentSpan->maxY - neighborSpan->maxY);
                    int maxClimbCells = static_cast<int>(config.stepHeight / config.cellHeight);
                    if (heightDiff <= maxClimbCells)
                    {
                        regionIds[nidx] = nextRegionId;
                        queue.push({nx, nz});
                    }
                }
            }
            
            if (cellCount > 0)
            {
                region.height = heightSum / cellCount;
            }
            
            // 最小面積チェック
            char dbg[128];
            snprintf(dbg, sizeof(dbg), "  Region %d: %zu cells", nextRegionId, region.cells.size());
            OutputDebugStringA(dbg);
            OutputDebugStringA("\n");
            
            if (static_cast<float>(region.cells.size()) >= config.minRegionArea)
            {
                outRegions.push_back(std::move(region));
                nextRegionId++;
            }
        }
    }
    
    snprintf(debugBuf, sizeof(debugBuf), "BuildRegions result: %zu regions passed minArea(%.1f)", 
             outRegions.size(), config.minRegionArea);
    OutputDebugStringA(debugBuf);
    OutputDebugStringA("\n");
    
    return !outRegions.empty();
}

bool NavMeshBuilder::BuildContours(const HeightField& heightField, const std::vector<Region>& regions,
                                    const NavMeshConfig& config, std::vector<Contour>& outContours)
{
    // 各領域の輪郭を抽出
    for (const auto& region : regions)
    {
        if (region.cells.empty())
            continue;
        
        Contour contour;
        contour.regionId = region.id;
        contour.height = region.height;
        
        // 簡易的な矩形輪郭を生成
        float minX = heightField.origin.x + region.minX * heightField.cellSize;
        float maxX = heightField.origin.x + (region.maxX + 1) * heightField.cellSize;
        float minZ = heightField.origin.z + region.minZ * heightField.cellSize;
        float maxZ = heightField.origin.z + (region.maxZ + 1) * heightField.cellSize;
        
        contour.vertices.push_back({minX, region.height, minZ});
        contour.vertices.push_back({maxX, region.height, minZ});
        contour.vertices.push_back({maxX, region.height, maxZ});
        contour.vertices.push_back({minX, region.height, maxZ});
        
        outContours.push_back(std::move(contour));
    }
    
    return !outContours.empty();
}

bool NavMeshBuilder::BuildPolygons(const std::vector<Contour>& contours, const NavMeshConfig& config, NavMeshData& outNavMesh)
{
    // 輪郭をポリゴンに変換（簡易版：各輪郭を1つのポリゴンとして扱う）
    for (const auto& contour : contours)
    {
        if (contour.vertices.size() < 3)
            continue;
        
        NavMeshPolygon polygon;
        uint32_t baseIndex = static_cast<uint32_t>(outNavMesh.vertices.size());
        
        for (const auto& v : contour.vertices)
        {
            outNavMesh.vertices.push_back(v);
            polygon.vertexIndices.push_back(baseIndex++);
        }
        
        polygon.neighbors.resize(polygon.vertexIndices.size(), NavMeshPolygon::INVALID_ID);
        polygon.center = CalculatePolygonCenter(outNavMesh, polygon);
        polygon.area = CalculatePolygonArea(outNavMesh, polygon);
        
        outNavMesh.polygons.push_back(std::move(polygon));
    }
    
    return !outNavMesh.polygons.empty();
}

void NavMeshBuilder::BuildNeighborConnections(NavMeshData& navMesh)
{
    // 隣接ポリゴンを検出（エッジを共有するポリゴン）
    for (size_t i = 0; i < navMesh.polygons.size(); ++i)
    {
        auto& polyA = navMesh.polygons[i];
        
        for (size_t j = i + 1; j < navMesh.polygons.size(); ++j)
        {
            auto& polyB = navMesh.polygons[j];
            
            // ポリゴンAの各エッジをチェック
            for (size_t edgeA = 0; edgeA < polyA.vertexIndices.size(); ++edgeA)
            {
                uint32_t a0 = polyA.vertexIndices[edgeA];
                uint32_t a1 = polyA.vertexIndices[(edgeA + 1) % polyA.vertexIndices.size()];
                
                // ポリゴンBの各エッジと比較
                for (size_t edgeB = 0; edgeB < polyB.vertexIndices.size(); ++edgeB)
                {
                    uint32_t b0 = polyB.vertexIndices[edgeB];
                    uint32_t b1 = polyB.vertexIndices[(edgeB + 1) % polyB.vertexIndices.size()];
                    
                    // 頂点が十分近いかチェック（共有エッジ）
                    const auto& va0 = navMesh.vertices[a0];
                    const auto& va1 = navMesh.vertices[a1];
                    const auto& vb0 = navMesh.vertices[b0];
                    const auto& vb1 = navMesh.vertices[b1];
                    
                    float epsilon = navMesh.config.cellSize * 0.1f;
                    
                    auto vertexMatch = [epsilon](const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2) {
                        return std::abs(p1.x - p2.x) < epsilon &&
                               std::abs(p1.y - p2.y) < epsilon &&
                               std::abs(p1.z - p2.z) < epsilon;
                    };
                    
                    // エッジが一致するか（方向は逆）
                    if ((vertexMatch(va0, vb1) && vertexMatch(va1, vb0)) ||
                        (vertexMatch(va0, vb0) && vertexMatch(va1, vb1)))
                    {
                        polyA.neighbors[edgeA] = static_cast<uint32_t>(j);
                        polyB.neighbors[edgeB] = static_cast<uint32_t>(i);
                    }
                }
            }
        }
    }
}

void NavMeshBuilder::ReportProgress(float progress, const char* stage)
{
    if (m_progressCallback)
    {
        m_progressCallback(progress, stage);
    }
}

DirectX::XMFLOAT3 NavMeshBuilder::CalculatePolygonCenter(const NavMeshData& navMesh, const NavMeshPolygon& polygon)
{
    DirectX::XMFLOAT3 center = {0.0f, 0.0f, 0.0f};
    if (polygon.vertexIndices.empty())
        return center;
    
    for (uint32_t idx : polygon.vertexIndices)
    {
        const auto& v = navMesh.vertices[idx];
        center.x += v.x;
        center.y += v.y;
        center.z += v.z;
    }
    
    float count = static_cast<float>(polygon.vertexIndices.size());
    center.x /= count;
    center.y /= count;
    center.z /= count;
    
    return center;
}

float NavMeshBuilder::CalculatePolygonArea(const NavMeshData& navMesh, const NavMeshPolygon& polygon)
{
    if (polygon.vertexIndices.size() < 3)
        return 0.0f;
    
    // Shoelace formula for polygon area (XZ plane)
    float area = 0.0f;
    size_t n = polygon.vertexIndices.size();
    
    for (size_t i = 0; i < n; ++i)
    {
        const auto& v0 = navMesh.vertices[polygon.vertexIndices[i]];
        const auto& v1 = navMesh.vertices[polygon.vertexIndices[(i + 1) % n]];
        area += (v0.x * v1.z) - (v1.x * v0.z);
    }
    
    return std::abs(area) * 0.5f;
}

} // namespace UnoEngine
