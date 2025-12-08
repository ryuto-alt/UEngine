#include "pch.h"
#include "NavMeshQuery.h"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <cmath>

namespace UnoEngine
{

NavMeshPath NavMeshQuery::FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal) const
{
    NavMeshPath result;
    
    if (!m_navMesh || m_navMesh->polygons.empty())
        return result;
    
    // 開始・終了ポリゴンを検索
    auto startPolyOpt = FindNearestPolygon(start);
    auto goalPolyOpt = FindNearestPolygon(goal);
    
    if (!startPolyOpt || !goalPolyOpt)
    {
        result.isPartial = true;
        return result;
    }
    
    uint32_t startPoly = *startPolyOpt;
    uint32_t goalPoly = *goalPolyOpt;
    
    // 同じポリゴン内なら直接移動
    if (startPoly == goalPoly)
    {
        result.waypoints.push_back(start);
        result.waypoints.push_back(goal);
        result.polygonPath.push_back(startPoly);
        return result;
    }
    
    // A*検索
    std::vector<PathNode> nodes(m_navMesh->polygons.size());
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i].polygonId = static_cast<uint32_t>(i);
        nodes[i].gCost = std::numeric_limits<float>::max();
        nodes[i].hCost = 0.0f;
        nodes[i].parent = NavMeshPolygon::INVALID_ID;
    }
    
    // 優先度付きキュー（fCostが小さい順）
    auto compare = [&nodes](uint32_t a, uint32_t b) {
        return nodes[a].fCost() > nodes[b].fCost();
    };
    std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(compare)> openSet(compare);
    std::unordered_set<uint32_t> closedSet;
    std::unordered_set<uint32_t> inOpenSet;
    
    // 開始ノード初期化
    nodes[startPoly].gCost = 0.0f;
    nodes[startPoly].hCost = Heuristic(m_navMesh->polygons[startPoly].center, 
                                        m_navMesh->polygons[goalPoly].center);
    openSet.push(startPoly);
    inOpenSet.insert(startPoly);
    
    uint32_t current = NavMeshPolygon::INVALID_ID;
    
    while (!openSet.empty())
    {
        current = openSet.top();
        openSet.pop();
        inOpenSet.erase(current);
        
        if (current == goalPoly)
            break;
        
        closedSet.insert(current);
        
        const auto& currentPoly = m_navMesh->polygons[current];
        
        // 隣接ポリゴンを探索
        for (size_t edge = 0; edge < currentPoly.neighbors.size(); ++edge)
        {
            uint32_t neighbor = currentPoly.neighbors[edge];
            if (neighbor == NavMeshPolygon::INVALID_ID)
                continue;
            
            if (closedSet.count(neighbor))
                continue;
            
            const auto& neighborPoly = m_navMesh->polygons[neighbor];
            
            // エッジの中点を通るコスト
            DirectX::XMFLOAT3 edgeMid = GetPortalMidpoint(current, neighbor);
            float edgeCost = Heuristic(currentPoly.center, edgeMid) + 
                            Heuristic(edgeMid, neighborPoly.center);
            
            float tentativeG = nodes[current].gCost + edgeCost;
            
            if (tentativeG < nodes[neighbor].gCost)
            {
                nodes[neighbor].parent = current;
                nodes[neighbor].gCost = tentativeG;
                nodes[neighbor].hCost = Heuristic(neighborPoly.center, 
                                                   m_navMesh->polygons[goalPoly].center);
                
                if (!inOpenSet.count(neighbor))
                {
                    openSet.push(neighbor);
                    inOpenSet.insert(neighbor);
                }
            }
        }
    }
    
    // パスを構築
    if (current != goalPoly)
    {
        result.isPartial = true;
        // 到達できなかった場合、最も近いノードまでのパスを返す
    }
    
    // パスを逆順で構築
    std::vector<uint32_t> polyPath;
    uint32_t node = current;
    while (node != NavMeshPolygon::INVALID_ID)
    {
        polyPath.push_back(node);
        node = nodes[node].parent;
    }
    std::reverse(polyPath.begin(), polyPath.end());
    
    result.polygonPath = polyPath;
    
    // ウェイポイント生成（ポリゴン中心を通る簡易版）
    result.waypoints.push_back(start);
    for (size_t i = 1; i < polyPath.size() - 1; ++i)
    {
        result.waypoints.push_back(m_navMesh->polygons[polyPath[i]].center);
    }
    if (current == goalPoly)
    {
        result.waypoints.push_back(goal);
    }
    else
    {
        result.waypoints.push_back(m_navMesh->polygons[current].center);
    }
    
    // String pullingでパスを最適化
    StringPull(result, start, current == goalPoly ? goal : m_navMesh->polygons[current].center);
    
    return result;
}

std::optional<uint32_t> NavMeshQuery::FindNearestPolygon(const DirectX::XMFLOAT3& point, float maxDistance) const
{
    if (!m_navMesh || m_navMesh->polygons.empty())
        return std::nullopt;
    
    uint32_t nearest = NavMeshPolygon::INVALID_ID;
    float minDist = maxDistance * maxDistance;
    
    for (size_t i = 0; i < m_navMesh->polygons.size(); ++i)
    {
        const auto& poly = m_navMesh->polygons[i];
        
        // ポリゴン中心との距離（XZ平面）
        float dx = point.x - poly.center.x;
        float dz = point.z - poly.center.z;
        float dist2D = dx * dx + dz * dz;
        
        // 高さ差も考慮
        float dy = point.y - poly.center.y;
        float dist3D = dist2D + dy * dy;
        
        if (dist3D < minDist)
        {
            // より正確な判定：ポリゴン内か近くにあるか
            if (IsPointInPolygon(point, static_cast<uint32_t>(i)))
            {
                return static_cast<uint32_t>(i);
            }
            
            minDist = dist3D;
            nearest = static_cast<uint32_t>(i);
        }
    }
    
    if (nearest != NavMeshPolygon::INVALID_ID && minDist <= maxDistance * maxDistance)
    {
        return nearest;
    }
    
    return std::nullopt;
}

bool NavMeshQuery::IsPointOnNavMesh(const DirectX::XMFLOAT3& point, float tolerance) const
{
    if (!m_navMesh)
        return false;
    
    for (size_t i = 0; i < m_navMesh->polygons.size(); ++i)
    {
        if (IsPointInPolygon(point, static_cast<uint32_t>(i)))
        {
            // 高さチェック
            float dy = std::abs(point.y - m_navMesh->polygons[i].center.y);
            if (dy <= tolerance)
                return true;
        }
    }
    
    return false;
}

std::optional<DirectX::XMFLOAT3> NavMeshQuery::SnapToNavMesh(const DirectX::XMFLOAT3& point, float maxDistance) const
{
    auto polyOpt = FindNearestPolygon(point, maxDistance);
    if (!polyOpt)
        return std::nullopt;
    
    return ClosestPointOnPolygon(point, *polyOpt);
}

bool NavMeshQuery::IsPointInPolygon(const DirectX::XMFLOAT3& point, uint32_t polygonId) const
{
    if (!m_navMesh || polygonId >= m_navMesh->polygons.size())
        return false;
    
    const auto& poly = m_navMesh->polygons[polygonId];
    if (poly.vertexIndices.size() < 3)
        return false;
    
    // 2D（XZ平面）で点がポリゴン内にあるか判定（Cross product法）
    size_t n = poly.vertexIndices.size();
    int sign = 0;
    
    for (size_t i = 0; i < n; ++i)
    {
        const auto& v0 = m_navMesh->vertices[poly.vertexIndices[i]];
        const auto& v1 = m_navMesh->vertices[poly.vertexIndices[(i + 1) % n]];
        
        float dx1 = v1.x - v0.x;
        float dz1 = v1.z - v0.z;
        float dx2 = point.x - v0.x;
        float dz2 = point.z - v0.z;
        
        float cross = dx1 * dz2 - dz1 * dx2;
        
        if (i == 0)
        {
            sign = (cross >= 0) ? 1 : -1;
        }
        else
        {
            if ((cross >= 0 ? 1 : -1) != sign)
                return false;
        }
    }
    
    return true;
}

bool NavMeshQuery::Raycast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, DirectX::XMFLOAT3& hitPoint) const
{
    // 簡易レイキャスト：NavMesh上で直線が通れるか
    auto startPolyOpt = FindNearestPolygon(start);
    if (!startPolyOpt)
    {
        hitPoint = start;
        return false;
    }
    
    // 終点がNavMesh上にあるか
    if (IsPointOnNavMesh(end))
    {
        hitPoint = end;
        return true;
    }
    
    hitPoint = end;
    return false;
}

float NavMeshQuery::Heuristic(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

DirectX::XMFLOAT3 NavMeshQuery::GetPortalMidpoint(uint32_t fromPoly, uint32_t toPoly) const
{
    if (!m_navMesh || fromPoly >= m_navMesh->polygons.size() || toPoly >= m_navMesh->polygons.size())
        return {};
    
    const auto& polyFrom = m_navMesh->polygons[fromPoly];
    const auto& polyTo = m_navMesh->polygons[toPoly];
    
    // 共有エッジを見つける
    for (size_t i = 0; i < polyFrom.vertexIndices.size(); ++i)
    {
        if (polyFrom.neighbors[i] == toPoly)
        {
            uint32_t v0 = polyFrom.vertexIndices[i];
            uint32_t v1 = polyFrom.vertexIndices[(i + 1) % polyFrom.vertexIndices.size()];
            
            const auto& p0 = m_navMesh->vertices[v0];
            const auto& p1 = m_navMesh->vertices[v1];
            
            return {
                (p0.x + p1.x) * 0.5f,
                (p0.y + p1.y) * 0.5f,
                (p0.z + p1.z) * 0.5f
            };
        }
    }
    
    // 見つからなければ両方の中心の中点
    return {
        (polyFrom.center.x + polyTo.center.x) * 0.5f,
        (polyFrom.center.y + polyTo.center.y) * 0.5f,
        (polyFrom.center.z + polyTo.center.z) * 0.5f
    };
}

DirectX::XMFLOAT3 NavMeshQuery::ClosestPointOnPolygon(const DirectX::XMFLOAT3& point, uint32_t polygonId) const
{
    if (!m_navMesh || polygonId >= m_navMesh->polygons.size())
        return point;
    
    const auto& poly = m_navMesh->polygons[polygonId];
    
    // ポリゴン内にあればそのまま（高さは調整）
    if (IsPointInPolygon(point, polygonId))
    {
        return {point.x, poly.center.y, point.z};
    }
    
    // ポリゴンの各辺に対して最近点を計算
    float minDist = std::numeric_limits<float>::max();
    DirectX::XMFLOAT3 closest = poly.center;
    
    size_t n = poly.vertexIndices.size();
    for (size_t i = 0; i < n; ++i)
    {
        const auto& v0 = m_navMesh->vertices[poly.vertexIndices[i]];
        const auto& v1 = m_navMesh->vertices[poly.vertexIndices[(i + 1) % n]];
        
        // 辺上の最近点を計算
        float edgeDx = v1.x - v0.x;
        float edgeDz = v1.z - v0.z;
        float edgeLenSq = edgeDx * edgeDx + edgeDz * edgeDz;
        
        if (edgeLenSq < 0.0001f)
            continue;
        
        float t = ((point.x - v0.x) * edgeDx + (point.z - v0.z) * edgeDz) / edgeLenSq;
        t = std::clamp(t, 0.0f, 1.0f);
        
        DirectX::XMFLOAT3 proj = {
            v0.x + t * edgeDx,
            v0.y + t * (v1.y - v0.y),
            v0.z + t * edgeDz
        };
        
        float dx = point.x - proj.x;
        float dz = point.z - proj.z;
        float dist = dx * dx + dz * dz;
        
        if (dist < minDist)
        {
            minDist = dist;
            closest = proj;
        }
    }
    
    return closest;
}

void NavMeshQuery::StringPull(NavMeshPath& path, const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal) const
{
    // 簡易的なString pulling（Funnel algorithm）
    // 現在の実装では詳細なファンネルアルゴリズムは省略
    // ポリゴン中心を通る経路を使用
    
    if (path.waypoints.size() <= 2)
        return;
    
    // 直接見通しがきく点を削除
    std::vector<DirectX::XMFLOAT3> optimized;
    optimized.push_back(path.waypoints[0]);
    
    size_t current = 0;
    while (current < path.waypoints.size() - 1)
    {
        size_t furthest = current + 1;
        
        // 現在位置から直接到達できる最も遠い点を探す
        for (size_t i = current + 2; i < path.waypoints.size(); ++i)
        {
            DirectX::XMFLOAT3 hit;
            if (Raycast(path.waypoints[current], path.waypoints[i], hit))
            {
                furthest = i;
            }
        }
        
        current = furthest;
        optimized.push_back(path.waypoints[current]);
    }
    
    path.waypoints = std::move(optimized);
}

} // namespace UnoEngine
