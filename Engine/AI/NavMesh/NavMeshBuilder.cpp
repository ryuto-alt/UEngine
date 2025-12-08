#include "pch.h"
#include "NavMeshBuilder.h"
#include "../../Core/Scene.h"
#include "../../Core/GameObject.h"
#include "../../Core/CollisionComponent.h"
#include "../../Graphics/MeshRenderer.h"
#include "../../Resource/StaticModelImporter.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <set>
#include <unordered_map>
#include <unordered_set>
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

// HeightField実装
void HeightField::Clear()
{
    for (auto span : spans)
    {
        while (span)
        {
            auto next = span->next;
            delete span;
            span = next;
        }
    }
    spans.clear();
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
    
    // 1.5. 障害物領域をカーブ（歩行不可としてマーク）
    if (!geometry.obstacleAABBs.empty())
    {
        snprintf(debugBuf, sizeof(debugBuf), "Carving %zu obstacles...", geometry.obstacleAABBs.size());
        ReportProgress(0.12f, debugBuf);
        CarveObstacles(geometry, config, heightField);
    }
    
    // スパン数をカウント
    int totalSpans = 0;
    int walkableSpans = 0;
    for (auto* span : heightField.spans)
    {
        while (span)
        {
            ++totalSpans;
            if (span->area > 0) ++walkableSpans;
            span = span->next;
        }
    }
    
    snprintf(debugBuf, sizeof(debugBuf), "Voxelized: %dx%d grid, %d spans, %d walkable",
             heightField.width, heightField.height, totalSpans, walkableSpans);
    ReportProgress(0.15f, debugBuf);
    
    // 2. 歩行可能領域のフィルタリング（デバッグ用に一時無効化）
    ReportProgress(0.2f, "Filtering walkable areas...");
    int walkableHeight = static_cast<int>(std::ceil(config.agentHeight / config.cellHeight));
    int walkableClimb = static_cast<int>(std::floor(config.stepHeight / config.cellHeight));
    // デバッグ：フィルタリングを一時的にスキップ
    // FilterWalkableLowHeightSpans(heightField, static_cast<float>(walkableHeight));
    // FilterLedgeSpans(heightField, static_cast<float>(walkableClimb));
    
    // フィルタリング後のスパン数
    walkableSpans = 0;
    for (auto* span : heightField.spans)
    {
        while (span)
        {
            if (span->area > 0) ++walkableSpans;
            span = span->next;
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
    
    // 5.5. デバッグ用にHeightFieldからグリッド情報を保存
    {
        auto& grid = navMesh->walkableGrid;
        grid.width = heightField.width;
        grid.height = heightField.height;
        grid.origin = heightField.origin;
        grid.cellSize = heightField.cellSize;
        grid.cells.resize(grid.width * grid.height, 0);
        
        float heightSum = 0.0f;
        int walkableCount = 0;
        
        for (int z = 0; z < grid.height; ++z)
        {
            for (int x = 0; x < grid.width; ++x)
            {
                int idx = z * grid.width + x;
                HeightSpan* span = heightField.spans[idx];
                
                // 最も上のスパンが歩行可能かチェック
                bool walkable = false;
                while (span)
                {
                    if (span->area > 0)
                    {
                        walkable = true;
                        heightSum += heightField.origin.y + span->maxY * heightField.cellHeight;
                        ++walkableCount;
                    }
                    span = span->next;
                }
                grid.cells[idx] = walkable ? 1 : 0;
            }
        }
        
        grid.avgHeight = (walkableCount > 0) ? (heightSum / walkableCount) : 0.0f;
        
        snprintf(debugBuf, sizeof(debugBuf), "WalkableGrid: %dx%d, walkable=%d, avgY=%.2f",
                 grid.width, grid.height, walkableCount, grid.avgHeight);
        ReportProgress(0.85f, debugBuf);
    }
    
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
    int walkableMeshes = 0;
    int walkableAABBs = 0;
    int obstacleColliders = 0;
    int totalTriangles = 0;
    
    for (const auto& gameObject : scene->GetGameObjects())
    {
        ++totalObjects;
        auto* collider = gameObject->GetComponent<CollisionComponent>();
        if (!collider || !collider->IsEnabled())
            continue;
        
        if (collider->IsNavMeshWalkable())
        {
            // MeshRendererがあれば実際のメッシュ三角形を使用
            auto* meshRenderer = gameObject->GetComponent<MeshRenderer>();
            if (meshRenderer && meshRenderer->HasModel())
            {
                auto* modelData = meshRenderer->GetModel();
                auto& transform = gameObject->GetTransform();
                Vector3 worldPos = transform.GetPosition();
                Vector3 worldScale = transform.GetScale();
                
                for (const auto& mesh : modelData->meshes)
                {
                    if (!mesh.HasCpuData())
                        continue;
                    
                    const auto& vertices = mesh.GetVertices();
                    const auto& indices = mesh.GetIndices();
                    
                    // 三角形を追加（ワールド座標に変換）
                    for (size_t i = 0; i + 2 < indices.size(); i += 3)
                    {
                        const auto& v0 = vertices[indices[i]];
                        const auto& v1 = vertices[indices[i + 1]];
                        const auto& v2 = vertices[indices[i + 2]];
                        
                        DirectX::XMFLOAT3 p0(
                            worldPos.GetX() + v0.px * worldScale.GetX(),
                            worldPos.GetY() + v0.py * worldScale.GetY(),
                            worldPos.GetZ() + v0.pz * worldScale.GetZ()
                        );
                        DirectX::XMFLOAT3 p1(
                            worldPos.GetX() + v1.px * worldScale.GetX(),
                            worldPos.GetY() + v1.py * worldScale.GetY(),
                            worldPos.GetZ() + v1.pz * worldScale.GetZ()
                        );
                        DirectX::XMFLOAT3 p2(
                            worldPos.GetX() + v2.px * worldScale.GetX(),
                            worldPos.GetY() + v2.py * worldScale.GetY(),
                            worldPos.GetZ() + v2.pz * worldScale.GetZ()
                        );
                        
                        outGeometry.AddTriangle(p0, p1, p2);
                        ++totalTriangles;
                    }
                    ++walkableMeshes;
                }
            }
            else
            {
                // メッシュがなければAABBを使用
                CollectGeometryFromCollider(collider, outGeometry);
                ++walkableAABBs;
            }
        }
        else if (collider->IsNavMeshObstacle())
        {
            ++obstacleColliders;
            if (collider->HasMultipleAABBs())
            {
                for (const auto& aabb : collider->GetWorldAABBs())
                {
                    outGeometry.AddObstacle(aabb);
                }
            }
            else
            {
                outGeometry.AddObstacle(collider->GetWorldAABB());
            }
        }
    }
    
    char debugMsg[256];
    snprintf(debugMsg, sizeof(debugMsg), "Found %d objects, %d meshes, %d AABBs, %d obstacles, %zu triangles", 
             totalObjects, walkableMeshes, walkableAABBs, obstacleColliders, outGeometry.indices.size() / 3);
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
    // ジオメトリのバウンディングボックスを計算（歩行可能領域 + 障害物を含む）
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
    
    // 障害物AABBも含めてバウンディングボックスを拡張
    for (const auto& obstacle : geometry.obstacleAABBs)
    {
        minPt.x = std::min(minPt.x, obstacle.min.GetX());
        minPt.z = std::min(minPt.z, obstacle.min.GetZ());
        maxPt.x = std::max(maxPt.x, obstacle.max.GetX());
        maxPt.z = std::max(maxPt.z, obstacle.max.GetZ());
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
    
    outHeightField.spans.resize(outHeightField.width * outHeightField.height, nullptr);
    
    // 最大傾斜角をラジアンに変換
    float maxSlopeRad = config.maxSlope * 3.14159265f / 180.0f;
    float minNormalY = std::cos(maxSlopeRad); // 上向き法線のY成分の最小値
    
    // 三角形をボクセル化
    for (size_t i = 0; i < geometry.indices.size(); i += 3)
    {
        const auto& v0 = geometry.vertices[geometry.indices[i]];
        const auto& v1 = geometry.vertices[geometry.indices[i + 1]];
        const auto& v2 = geometry.vertices[geometry.indices[i + 2]];
        
        // 三角形の法線を計算
        float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
        float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0001f)
        {
            ny /= len; // 正規化されたY成分のみ必要
        }
        
        // 上向きでない面（壁や天井）はスキップ
        if (ny < minNormalY)
            continue;
        
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
                
                auto* span = new HeightSpan();
                span->minY = minYCell;
                span->maxY = maxYCell;
                span->area = 1; // 歩行可能
                span->next = outHeightField.spans[idx];
                outHeightField.spans[idx] = span;
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
            HeightSpan* span = heightField.spans[idx];
            
            while (span)
            {
                HeightSpan* next = span->next;
                
                // 次のスパンとの間隔が十分あるか確認
                if (next)
                {
                    int gap = next->minY - span->maxY;
                    if (gap < walkableHeightCells)
                    {
                        span->area = 0; // 歩行不可
                    }
                }
                
                span = next;
            }
        }
    }
}

void NavMeshBuilder::FilterLedgeSpans(HeightField& heightField, float walkableClimb)
{
    int maxClimbCells = static_cast<int>(walkableClimb);
    int dx[] = {-1, 0, 1, 0};
    int dz[] = {0, -1, 0, 1};
    
    for (int z = 0; z < heightField.height; ++z)
    {
        for (int x = 0; x < heightField.width; ++x)
        {
            int idx = z * heightField.width + x;
            HeightSpan* span = heightField.spans[idx];
            
            while (span)
            {
                if (span->area == 0)
                {
                    span = span->next;
                    continue;
                }
                
                // 隣接セルとの高さ差をチェック（エッジは許容）
                int invalidNeighbors = 0;
                for (int dir = 0; dir < 4; ++dir)
                {
                    int nx = x + dx[dir];
                    int nz = z + dz[dir];
                    
                    // グリッド外は単にスキップ（エッジは歩行可能）
                    if (nx < 0 || nx >= heightField.width || nz < 0 || nz >= heightField.height)
                        continue;
                    
                    int nidx = nz * heightField.width + nx;
                    HeightSpan* neighborSpan = heightField.spans[nidx];
                    
                    // 隣接スパンで最も近い高さを探す
                    bool hasValidNeighbor = false;
                    while (neighborSpan)
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
                        neighborSpan = neighborSpan->next;
                    }
                    
                    if (!hasValidNeighbor)
                    {
                        ++invalidNeighbors;
                    }
                }
                
                // 全ての有効な隣接セルが段差が大きすぎる場合のみ歩行不可
                // （孤立したセルは歩行不可）
                if (invalidNeighbors >= 4)
                {
                    span->area = 0;
                }
                
                span = span->next;
            }
        }
    }
}

void NavMeshBuilder::CarveObstacles(const NavMeshInputGeometry& geometry, const NavMeshConfig& config, HeightField& heightField)
{
    int carvedCells = 0;
    int obstacleIdx = 0;
    
    char debugBuf[256];
    snprintf(debugBuf, sizeof(debugBuf), "HeightField origin: (%.1f, %.1f, %.1f), size: %dx%d, cellSize: %.2f",
             heightField.origin.x, heightField.origin.y, heightField.origin.z,
             heightField.width, heightField.height, heightField.cellSize);
    OutputDebugStringA(debugBuf);
    OutputDebugStringA("\n");
    
    for (const auto& obstacle : geometry.obstacleAABBs)
    {
        // 障害物AABBをセル座標に変換（XZ平面のみ考慮）
        float minX = obstacle.min.GetX();
        float maxX = obstacle.max.GetX();
        float minZ = obstacle.min.GetZ();
        float maxZ = obstacle.max.GetZ();
        
        // デバッグ: 最初の5つと最後の5つの障害物をログ出力
        if (obstacleIdx < 5 || obstacleIdx >= static_cast<int>(geometry.obstacleAABBs.size()) - 5)
        {
            snprintf(debugBuf, sizeof(debugBuf), "Obstacle[%d]: X(%.1f~%.1f) Z(%.1f~%.1f)",
                     obstacleIdx, minX, maxX, minZ, maxZ);
            OutputDebugStringA(debugBuf);
            OutputDebugStringA("\n");
        }
        
        // セル範囲を計算
        int x0 = static_cast<int>((minX - heightField.origin.x) / heightField.cellSize);
        int x1 = static_cast<int>((maxX - heightField.origin.x) / heightField.cellSize);
        int z0 = static_cast<int>((minZ - heightField.origin.z) / heightField.cellSize);
        int z1 = static_cast<int>((maxZ - heightField.origin.z) / heightField.cellSize);
        
        // クランプ
        x0 = std::max(0, x0);
        x1 = std::min(heightField.width - 1, x1);
        z0 = std::max(0, z0);
        z1 = std::min(heightField.height - 1, z1);
        
        int cellsThisObstacle = 0;
        
        // 障害物範囲内のスパンを全て歩行不可に設定（Y座標は無視）
        for (int z = z0; z <= z1; ++z)
        {
            for (int x = x0; x <= x1; ++x)
            {
                int idx = z * heightField.width + x;
                HeightSpan* span = heightField.spans[idx];
                
                while (span)
                {
                    if (span->area > 0)
                    {
                        span->area = 0; // 歩行不可
                        ++carvedCells;
                        ++cellsThisObstacle;
                    }
                    span = span->next;
                }
            }
        }
        
        if (obstacleIdx < 5 || obstacleIdx >= static_cast<int>(geometry.obstacleAABBs.size()) - 5)
        {
            snprintf(debugBuf, sizeof(debugBuf), "  -> cells: x(%d~%d) z(%d~%d), carved: %d",
                     x0, x1, z0, z1, cellsThisObstacle);
            OutputDebugStringA(debugBuf);
            OutputDebugStringA("\n");
        }
        
        ++obstacleIdx;
    }
    
    snprintf(debugBuf, sizeof(debugBuf), "Carved %d cells from %zu obstacles", carvedCells, geometry.obstacleAABBs.size());
    ReportProgress(0.14f, debugBuf);
}

bool NavMeshBuilder::BuildRegions(HeightField& heightField, const NavMeshConfig& config, std::vector<Region>& outRegions)
{
    // 単純なフラッドフィル領域分割
    std::vector<uint32_t> regionIds(heightField.width * heightField.height, 0);
    uint32_t nextRegionId = 1;
    
    // デバッグ：歩行可能セル数をカウント
    int walkableCellCount = 0;
    for (int i = 0; i < heightField.width * heightField.height; ++i)
    {
        HeightSpan* span = heightField.spans[i];
        if (span && span->area > 0)
            ++walkableCellCount;
    }
    char debugBuf[256];
    snprintf(debugBuf, sizeof(debugBuf), "BuildRegions: grid=%dx%d, walkable cells=%d", 
             heightField.width, heightField.height, walkableCellCount);
    OutputDebugStringA(debugBuf);
    OutputDebugStringA("\n");
    
    int dx[] = {-1, 0, 1, 0};
    int dz[] = {0, -1, 0, 1};
    
    for (int z = 0; z < heightField.height; ++z)
    {
        for (int x = 0; x < heightField.width; ++x)
        {
            int idx = z * heightField.width + x;
            HeightSpan* span = heightField.spans[idx];
            
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
                HeightSpan* currentSpan = heightField.spans[cidx];
                
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
                    
                    HeightSpan* neighborSpan = heightField.spans[nidx];
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
    // Greedy Row-Run: 行方向にマージして矩形を生成
    auto cellHash = [](const std::pair<int, int>& p) {
        return std::hash<int64_t>{}((static_cast<int64_t>(p.first) << 32) | static_cast<uint32_t>(p.second));
    };

    for (const auto& region : regions)
    {
        if (region.cells.empty())
            continue;

        std::vector<std::pair<int, int>> sortedCells = region.cells;
        std::sort(sortedCells.begin(), sortedCells.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second < b.second;
            return a.first < b.first;
        });

        std::unordered_set<std::pair<int, int>, decltype(cellHash)> cellSet(sortedCells.begin(), sortedCells.end(), 0, cellHash);
        std::unordered_set<std::pair<int, int>, decltype(cellHash)> processed(0, cellHash);

        for (const auto& [cx, cz] : sortedCells)
        {
            if (processed.count({cx, cz}))
                continue;

            int width = 0;
            while (cellSet.count({cx + width, cz}) && !processed.count({cx + width, cz}))
                ++width;

            if (width == 0) continue;

            int height = 1;
            while (true)
            {
                bool canExtend = true;
                for (int x = cx; x < cx + width; ++x)
                {
                    if (!cellSet.count({x, cz + height}) || processed.count({x, cz + height}))
                    {
                        canExtend = false;
                        break;
                    }
                }
                if (!canExtend) break;
                ++height;
            }

            for (int z = cz; z < cz + height; ++z)
                for (int x = cx; x < cx + width; ++x)
                    processed.insert({x, z});

            Contour contour;
            contour.regionId = region.id;
            contour.height = region.height;

            float minX = heightField.origin.x + cx * heightField.cellSize;
            float maxX = heightField.origin.x + (cx + width) * heightField.cellSize;
            float minZ = heightField.origin.z + cz * heightField.cellSize;
            float maxZ = heightField.origin.z + (cz + height) * heightField.cellSize;

            contour.vertices.push_back({minX, region.height, minZ});
            contour.vertices.push_back({maxX, region.height, minZ});
            contour.vertices.push_back({maxX, region.height, maxZ});
            contour.vertices.push_back({minX, region.height, maxZ});

            outContours.push_back(std::move(contour));
        }
    }

    char debugBuf[128];
    snprintf(debugBuf, sizeof(debugBuf), "BuildContours: %zu rectangles", outContours.size());
    OutputDebugStringA(debugBuf);
    OutputDebugStringA("\n");

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
    // AABBベース隣接検出（パスファインディング用）
    float eps = navMesh.config.cellSize * 0.5f;

    struct Rect { float minX, maxX, minZ, maxZ; };
    std::vector<Rect> rects(navMesh.polygons.size());

    for (size_t i = 0; i < navMesh.polygons.size(); ++i)
    {
        auto& r = rects[i];
        r.minX = FLT_MAX; r.maxX = -FLT_MAX;
        r.minZ = FLT_MAX; r.maxZ = -FLT_MAX;
        for (uint32_t vi : navMesh.polygons[i].vertexIndices)
        {
            r.minX = std::min(r.minX, navMesh.vertices[vi].x);
            r.maxX = std::max(r.maxX, navMesh.vertices[vi].x);
            r.minZ = std::min(r.minZ, navMesh.vertices[vi].z);
            r.maxZ = std::max(r.maxZ, navMesh.vertices[vi].z);
        }
    }

    int connections = 0;
    for (size_t i = 0; i < navMesh.polygons.size(); ++i)
    {
        for (size_t j = i + 1; j < navMesh.polygons.size(); ++j)
        {
            const auto& a = rects[i];
            const auto& b = rects[j];

            bool xTouch = std::abs(a.maxX - b.minX) < eps || std::abs(a.minX - b.maxX) < eps;
            bool zTouch = std::abs(a.maxZ - b.minZ) < eps || std::abs(a.minZ - b.maxZ) < eps;
            bool xOverlap = (a.maxX > b.minX) && (a.minX < b.maxX);
            bool zOverlap = (a.maxZ > b.minZ) && (a.minZ < b.maxZ);

            if ((xTouch && zOverlap) || (zTouch && xOverlap))
            {
                // 簡易接続（詳細なエッジ情報は描画で使わないので省略）
                auto& polyA = navMesh.polygons[i];
                auto& polyB = navMesh.polygons[j];

                for (auto& n : polyA.neighbors)
                    if (n == NavMeshPolygon::INVALID_ID) { n = static_cast<uint32_t>(j); break; }
                for (auto& n : polyB.neighbors)
                    if (n == NavMeshPolygon::INVALID_ID) { n = static_cast<uint32_t>(i); break; }
                ++connections;
            }
        }
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Neighbors: %zu polys, %d connections", navMesh.polygons.size(), connections);
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
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
