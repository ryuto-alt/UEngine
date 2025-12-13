#include "pch.h"
#include "NavMeshSystem.h"
#include "../../Rendering/DebugRenderer.h"
#include "../../Math/Vector.h"
#include <cfloat>

namespace {
    // XMFLOAT3 -> Vector3 変換ヘルパー
    UnoEngine::Vector3 ToVector3(const DirectX::XMFLOAT3& v) {
        return UnoEngine::Vector3(v.x, v.y, v.z);
    }
    UnoEngine::Vector4 ToVector4(const DirectX::XMFLOAT4& v) {
        return UnoEngine::Vector4(v.x, v.y, v.z, v.w);
    }
}

namespace UnoEngine
{

NavMeshSystem& NavMeshSystem::GetInstance()
{
    static NavMeshSystem instance;
    return instance;
}

bool NavMeshSystem::BakeNavMesh(Scene* scene)
{
    return BakeNavMesh(scene, m_config);
}

bool NavMeshSystem::BakeNavMesh(Scene* scene, const NavMeshConfig& config)
{
    if (!scene)
        return false;
    
    NavMeshBuilder builder;
    builder.SetProgressCallback(m_progressCallback);
    
    m_navMesh = builder.Build(scene, config);
    
    if (m_navMesh && m_navMesh->IsValid())
    {
        m_query.SetNavMesh(m_navMesh.get());
        m_config = config;
        return true;
    }
    
    return false;
}

void NavMeshSystem::ClearNavMesh()
{
    m_navMesh.reset();
    m_query.SetNavMesh(nullptr);
}

NavMeshPath NavMeshSystem::FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal) const
{
    return m_query.FindPath(start, goal);
}

std::optional<uint32_t> NavMeshSystem::FindNearestPolygon(const DirectX::XMFLOAT3& point, float maxDistance) const
{
    return m_query.FindNearestPolygon(point, maxDistance);
}

bool NavMeshSystem::IsPointOnNavMesh(const DirectX::XMFLOAT3& point) const
{
    return m_query.IsPointOnNavMesh(point);
}

std::optional<DirectX::XMFLOAT3> NavMeshSystem::SnapToNavMesh(const DirectX::XMFLOAT3& point, float maxDistance) const
{
    return m_query.SnapToNavMesh(point, maxDistance);
}

bool NavMeshSystem::SaveNavMesh(const std::filesystem::path& filePath)
{
    if (!m_navMesh || !m_navMesh->IsValid())
        return false;
    
    return NavMeshSerializer::Save(*m_navMesh, filePath);
}

bool NavMeshSystem::LoadNavMesh(const std::filesystem::path& filePath)
{
    auto loaded = NavMeshSerializer::Load(filePath);
    if (!loaded || !loaded->IsValid())
        return false;
    
    m_navMesh = std::move(loaded);
    m_query.SetNavMesh(m_navMesh.get());
    m_config = m_navMesh->config;
    
    return true;
}

void NavMeshSystem::DrawDebug(DebugRenderer* debugRenderer) const
{
    if (!debugRenderer || !m_debugDrawEnabled || !m_navMesh)
        return;

    // ポリゴンを緑色で塗りつぶし描画
    const auto& vertices = m_navMesh->vertices;
    const auto& polygons = m_navMesh->polygons;
    
    if (!vertices.empty() && !polygons.empty())
    {
        Vector4 fillColor(0.0f, 0.8f, 0.3f, 0.35f);
        constexpr float yOffset = 0.02f;

        for (const auto& poly : polygons)
        {
            if (poly.vertexIndices.size() < 3)
                continue;

            // Fan三角形分割
            const auto& idx = poly.vertexIndices;
            const auto& v0 = vertices[idx[0]];
            Vector3 p0(v0.x, v0.y + yOffset, v0.z);

            for (size_t i = 1; i + 1 < idx.size(); ++i)
            {
                const auto& v1 = vertices[idx[i]];
                const auto& v2 = vertices[idx[i + 1]];
                Vector3 p1(v1.x, v1.y + yOffset, v1.z);
                Vector3 p2(v2.x, v2.y + yOffset, v2.z);
                debugRenderer->AddTriangle(p0, p1, p2, fillColor);
            }
        }
    }

    // 外周ラインを描画
    const auto& grid = m_navMesh->walkableGrid;
    if (!grid.boundaryLines.empty())
    {
        Vector4 edgeColor(0.0f, 1.0f, 0.5f, 0.9f);

        for (size_t i = 0; i + 1 < grid.boundaryLines.size(); i += 2)
        {
            const auto& p0 = grid.boundaryLines[i];
            const auto& p1 = grid.boundaryLines[i + 1];
            debugRenderer->AddLine(Vector3(p0.x, p0.y, p0.z), Vector3(p1.x, p1.y, p1.z), edgeColor);
        }
    }
}

void NavMeshSystem::DrawPath(DebugRenderer* debugRenderer, const NavMeshPath& path,
                              const DirectX::XMFLOAT4& color) const
{
    if (!debugRenderer || !path.IsValid())
        return;
    
    Vector4 lineColor = ToVector4(color);
    
    // パスのウェイポイントを線で結ぶ
    for (size_t i = 0; i < path.waypoints.size() - 1; ++i)
    {
        const auto& wp0 = path.waypoints[i];
        const auto& wp1 = path.waypoints[i + 1];
        
        // 少し上にオフセット
        Vector3 p0(wp0.x, wp0.y + 0.15f, wp0.z);
        Vector3 p1(wp1.x, wp1.y + 0.15f, wp1.z);
        
        debugRenderer->AddLine(p0, p1, lineColor);
    }
    
    // ウェイポイントに小さな球を描画
    Vector4 waypointColor(color.x, color.y, color.z, 1.0f);
    for (const auto& wp : path.waypoints)
    {
        Vector3 p(wp.x, wp.y + 0.15f, wp.z);
        debugRenderer->AddSphere(p, 0.1f, waypointColor);
    }
}

} // namespace UnoEngine
