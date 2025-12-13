#include "pch.h"
#include "NavMeshSystem.h"
#include "../../Core/Scene.h"
#include "../../Core/GameObject.h"
#include "../../Core/Transform.h"
#include "../../Core/CollisionComponent.h"
#include "../../Graphics/MeshRenderer.h"
#include "../../Resource/StaticModelImporter.h"
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

void NavMeshSystem::BakeNavMeshAsync(Scene* scene)
{
    BakeNavMeshAsync(scene, m_config);
}

void NavMeshSystem::BakeNavMeshAsync(Scene* scene, const NavMeshConfig& config)
{
    if (!scene || m_isBaking.load())
        return;
    
    // 前のスレッドが残っていれば待機
    if (m_bakeThread && m_bakeThread->joinable())
    {
        m_bakeThread->join();
    }
    
    m_isBaking.store(true);
    m_bakeComplete.store(false);
    m_bakeSuccess.store(false);
    m_bakeProgress.store(0.0f);
    snprintf(m_bakeStage, sizeof(m_bakeStage), "Starting...");
    
    // シーンからジオメトリを事前収集（メインスレッドで）
    NavMeshBuilder preBuilder;
    NavMeshInputGeometry geometry;
    preBuilder.SetProgressCallback([this](float progress, const char* stage) {
        m_bakeProgress.store(progress);
        snprintf(m_bakeStage, sizeof(m_bakeStage), "%s", stage);
    });
    
    // ジオメトリ収集はシーンアクセスが必要なのでメインスレッドで
    for (const auto& gameObject : scene->GetGameObjects())
    {
        auto* collider = gameObject->GetComponent<CollisionComponent>();
        if (!collider || !collider->IsEnabled())
            continue;
        
        if (collider->IsNavMeshWalkable())
        {
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
                        
                        geometry.AddTriangle(p0, p1, p2);
                    }
                }
            }
            else
            {
                if (collider->HasMultipleAABBs())
                {
                    for (const auto& aabb : collider->GetWorldAABBs())
                        geometry.AddAABB(aabb);
                }
                else
                {
                    geometry.AddAABB(collider->GetWorldAABB());
                }
            }
        }
        else if (collider->IsNavMeshObstacle())
        {
            if (collider->HasMultipleAABBs())
            {
                for (const auto& aabb : collider->GetWorldAABBs())
                    geometry.AddObstacle(aabb);
            }
            else
            {
                geometry.AddObstacle(collider->GetWorldAABB());
            }
        }
    }
    
    if (geometry.IsEmpty())
    {
        m_isBaking.store(false);
        m_bakeComplete.store(true);
        m_bakeSuccess.store(false);
        snprintf(m_bakeStage, sizeof(m_bakeStage), "No geometry found");
        return;
    }
    
    // 重い処理を別スレッドで実行
    NavMeshConfig configCopy = config;
    m_bakeThread = std::make_unique<std::thread>([this, geometry = std::move(geometry), configCopy]() {
        NavMeshBuilder builder;
        builder.SetProgressCallback([this](float progress, const char* stage) {
            m_bakeProgress.store(progress);
            snprintf(m_bakeStage, sizeof(m_bakeStage), "%s", stage);
        });
        
        auto result = builder.Build(geometry, configCopy);
        
        {
            std::lock_guard<std::mutex> lock(m_bakeMutex);
            m_pendingNavMesh = std::move(result);
            m_bakeSuccess.store(m_pendingNavMesh && m_pendingNavMesh->IsValid());
        }
        
        m_bakeComplete.store(true);
        m_isBaking.store(false);
        snprintf(m_bakeStage, sizeof(m_bakeStage), m_bakeSuccess.load() ? "Complete!" : "Failed");
    });
}

bool NavMeshSystem::FinishBakeIfReady()
{
    if (!m_bakeComplete.load())
        return false;
    
    if (m_bakeThread && m_bakeThread->joinable())
    {
        m_bakeThread->join();
        m_bakeThread.reset();
    }
    
    bool success = false;
    {
        std::lock_guard<std::mutex> lock(m_bakeMutex);
        if (m_pendingNavMesh && m_pendingNavMesh->IsValid())
        {
            m_navMesh = std::move(m_pendingNavMesh);
            m_query.SetNavMesh(m_navMesh.get());
            success = true;
        }
        m_pendingNavMesh.reset();
    }
    
    m_bakeComplete.store(false);
    return success;
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

    // UIで設定した色を使用
    const auto& baseColor = m_debugDrawColor;
    Vector4 fillColor(baseColor.x, baseColor.y, baseColor.z, baseColor.w * 0.4f);
    Vector4 edgeColor(baseColor.x, baseColor.y, baseColor.z, 0.9f);

    // ポリゴンを塗りつぶし描画
    const auto& vertices = m_navMesh->vertices;
    const auto& polygons = m_navMesh->polygons;
    
    if (!vertices.empty() && !polygons.empty())
    {
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
