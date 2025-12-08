#pragma once

#include "NavMeshTypes.h"
#include <optional>

namespace UnoEngine
{

// NavMeshクエリクラス - パス検索や点の検索を提供
class NavMeshQuery
{
public:
    NavMeshQuery() = default;
    ~NavMeshQuery() = default;
    
    // NavMeshデータを設定
    void SetNavMesh(const NavMeshData* navMesh) { m_navMesh = navMesh; }
    const NavMeshData* GetNavMesh() const { return m_navMesh; }
    
    // パス検索 (A*)
    NavMeshPath FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal) const;
    
    // 指定した点に最も近いポリゴンを検索
    std::optional<uint32_t> FindNearestPolygon(const DirectX::XMFLOAT3& point, float maxDistance = 10.0f) const;
    
    // 指定した点がNavMesh上にあるか
    bool IsPointOnNavMesh(const DirectX::XMFLOAT3& point, float tolerance = 0.1f) const;
    
    // 指定した点を最も近いNavMesh上の点にスナップ
    std::optional<DirectX::XMFLOAT3> SnapToNavMesh(const DirectX::XMFLOAT3& point, float maxDistance = 10.0f) const;
    
    // ポリゴン内の点か判定
    bool IsPointInPolygon(const DirectX::XMFLOAT3& point, uint32_t polygonId) const;
    
    // レイキャスト（NavMesh上で直線が通れるか）
    bool Raycast(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, DirectX::XMFLOAT3& hitPoint) const;
    
private:
    // A*用のノード
    struct PathNode
    {
        uint32_t polygonId = 0;
        float gCost = 0.0f;   // 開始地点からのコスト
        float hCost = 0.0f;   // ゴールまでの推定コスト
        float fCost() const { return gCost + hCost; }
        uint32_t parent = NavMeshPolygon::INVALID_ID;
    };
    
    // ヒューリスティック関数（ユークリッド距離）
    float Heuristic(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const;
    
    // ポリゴン間のエッジ中点を取得
    DirectX::XMFLOAT3 GetPortalMidpoint(uint32_t fromPoly, uint32_t toPoly) const;
    
    // ポリゴン上の最も近い点を取得
    DirectX::XMFLOAT3 ClosestPointOnPolygon(const DirectX::XMFLOAT3& point, uint32_t polygonId) const;
    
    // String pulling（ファンネルアルゴリズム）でパスを最適化
    void StringPull(NavMeshPath& path, const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal) const;
    
    const NavMeshData* m_navMesh = nullptr;
};

} // namespace UnoEngine
