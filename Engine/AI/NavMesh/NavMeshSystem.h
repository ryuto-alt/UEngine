#pragma once

#include "NavMeshTypes.h"
#include "NavMeshBuilder.h"
#include "NavMeshQuery.h"
#include "NavMeshSerializer.h"
#include <memory>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>

namespace UnoEngine
{

class Scene;

// NavMeshシステム - シーンとの統合を管理
class NavMeshSystem
{
public:
    static NavMeshSystem& GetInstance();
    
    // NavMesh生成（同期）
    bool BakeNavMesh(Scene* scene);
    bool BakeNavMesh(Scene* scene, const NavMeshConfig& config);
    
    // NavMesh生成（非同期）
    void BakeNavMeshAsync(Scene* scene);
    void BakeNavMeshAsync(Scene* scene, const NavMeshConfig& config);
    bool IsBaking() const { return m_isBaking.load(); }
    float GetBakeProgress() const { return m_bakeProgress.load(); }
    const char* GetBakeStage() const { return m_bakeStage; }
    bool FinishBakeIfReady();  // 完了時にNavMeshを適用、成功ならtrue
    
    // NavMesh管理
    void ClearNavMesh();
    bool HasNavMesh() const { return m_navMesh != nullptr && m_navMesh->IsValid(); }
    const NavMeshData* GetNavMesh() const { return m_navMesh.get(); }
    
    // パス検索
    NavMeshPath FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal) const;
    
    // 点のクエリ
    std::optional<uint32_t> FindNearestPolygon(const DirectX::XMFLOAT3& point, float maxDistance = 10.0f) const;
    bool IsPointOnNavMesh(const DirectX::XMFLOAT3& point) const;
    std::optional<DirectX::XMFLOAT3> SnapToNavMesh(const DirectX::XMFLOAT3& point, float maxDistance = 10.0f) const;
    
    // ランダムな点の取得
    std::optional<DirectX::XMFLOAT3> FindRandomPoint() const;
    std::optional<DirectX::XMFLOAT3> FindRandomPointInRadius(const DirectX::XMFLOAT3& center, float radius) const;
    
    // ファイル操作
    bool SaveNavMesh(const std::filesystem::path& filePath);
    bool LoadNavMesh(const std::filesystem::path& filePath);
    
    // 設定の保存/読み込み
    void SaveSettings(const std::filesystem::path& filePath = "navmesh_settings.json");
    void LoadSettings(const std::filesystem::path& filePath = "navmesh_settings.json");
    
    // 設定
    NavMeshConfig& GetConfig() { return m_config; }
    const NavMeshConfig& GetConfig() const { return m_config; }
    void SetConfig(const NavMeshConfig& config) { m_config = config; }
    
    // 進捗コールバック（エディター用）
    using ProgressCallback = std::function<void(float progress, const char* stage)>;
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = std::move(callback); }
    
    // デバッグ情報
    uint32_t GetPolygonCount() const { return m_navMesh ? static_cast<uint32_t>(m_navMesh->polygons.size()) : 0; }
    uint32_t GetVertexCount() const { return m_navMesh ? static_cast<uint32_t>(m_navMesh->vertices.size()) : 0; }
    
    // デバッグ描画設定
    void SetDebugDrawEnabled(bool enabled) { m_debugDrawEnabled = enabled; }
    bool IsDebugDrawEnabled() const { return m_debugDrawEnabled; }
    void SetDebugDrawColor(const DirectX::XMFLOAT4& color) { m_debugDrawColor = color; }
    const DirectX::XMFLOAT4& GetDebugDrawColor() const { return m_debugDrawColor; }
    
    // デバッグ描画（DebugRendererを使用）
    void DrawDebug(class DebugRenderer* debugRenderer) const;
    void DrawPath(class DebugRenderer* debugRenderer, const NavMeshPath& path,
                  const DirectX::XMFLOAT4& color = {1.0f, 1.0f, 0.0f, 1.0f}) const;
    
private:
    NavMeshSystem() = default;
    ~NavMeshSystem() = default;
    NavMeshSystem(const NavMeshSystem&) = delete;
    NavMeshSystem& operator=(const NavMeshSystem&) = delete;
    
    std::unique_ptr<NavMeshData> m_navMesh;
    NavMeshQuery m_query;
    NavMeshConfig m_config;
    ProgressCallback m_progressCallback;
    
    // 非同期ベイク用
    std::atomic<bool> m_isBaking{false};
    std::atomic<float> m_bakeProgress{0.0f};
    char m_bakeStage[128] = "";
    std::unique_ptr<std::thread> m_bakeThread;
    std::unique_ptr<NavMeshData> m_pendingNavMesh;
    std::atomic<bool> m_bakeComplete{false};
    std::atomic<bool> m_bakeSuccess{false};
    std::mutex m_bakeMutex;
    
    bool m_debugDrawEnabled = false;
    DirectX::XMFLOAT4 m_debugDrawColor = {0.0f, 0.8f, 0.4f, 0.7f}; // 緑色
};

} // namespace UnoEngine
