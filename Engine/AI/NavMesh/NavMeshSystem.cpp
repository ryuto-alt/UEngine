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
    
    // ポリゴンベースの描画
    Vector4 fillColor(0.0f, 0.6f, 1.0f, 0.4f);
    Vector4 edgeColor(0.0f, 1.0f, 0.5f, 0.9f);
    float yOffset = 0.08f;
    
    for (const auto& poly : m_navMesh->polygons)
    {
        if (poly.vertexIndices.size() < 3)
            continue;
        
        // ポリゴンの輪郭を描画
        for (size_t i = 0; i < poly.vertexIndices.size(); ++i)
        {
            size_t nextIdx = (i + 1) % poly.vertexIndices.size();
            const auto& v0 = m_navMesh->vertices[poly.vertexIndices[i]];
            const auto& v1 = m_navMesh->vertices[poly.vertexIndices[nextIdx]];
            
            Vector3 p0(v0.x, v0.y + yOffset, v0.z);
            Vector3 p1(v1.x, v1.y + yOffset, v1.z);
            
            debugRenderer->AddLine(p0, p1, edgeColor);
        }
        
        // 中心点に十字を描画
        float cx = poly.center.x, cy = poly.center.y + yOffset, cz = poly.center.z;
        debugRenderer->AddLine(Vector3(cx - 0.2f, cy, cz), Vector3(cx + 0.2f, cy, cz), fillColor);
        debugRenderer->AddLine(Vector3(cx, cy, cz - 0.2f), Vector3(cx, cy, cz + 0.2f), fillColor);
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
