#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>

namespace UnoEngine
{

// NavMesh生成パラメータ
struct NavMeshConfig
{
    float agentRadius = 0.5f;      // エージェント半径
    float agentHeight = 2.0f;      // エージェント高さ
    float maxSlope = 45.0f;        // 最大登坂角度（度）
    float stepHeight = 0.3f;       // 登れる段差の高さ
    float cellSize = 0.3f;         // ボクセル化のセルサイズ (XZ)
    float cellHeight = 0.2f;       // ボクセル化の高さ分解能 (Y)
    float minRegionArea = 1.0f;    // 最小領域面積（セル単位）
    float mergeRegionArea = 20.0f; // マージする領域面積
    int maxEdgeLength = 12;        // 最大エッジ長（セル単位）
    float maxEdgeError = 1.3f;     // エッジ簡略化の最大誤差
    int vertsPerPoly = 6;          // ポリゴンあたりの最大頂点数
};

// NavMeshポリゴン
struct NavMeshPolygon
{
    std::vector<uint32_t> vertexIndices;  // 頂点インデックス（時計回り）
    std::vector<uint32_t> neighbors;      // 隣接ポリゴンID（辺ごと、なければINVALID_ID）
    DirectX::XMFLOAT3 center{};           // 重心
    float area = 0.0f;                    // 面積
    uint16_t flags = 0;                   // カスタムフラグ（領域タイプなど）
    
    static constexpr uint32_t INVALID_ID = std::numeric_limits<uint32_t>::max();
};

// デバッグ用歩行可能グリッド
struct WalkableGridData
{
    std::vector<uint8_t> cells;  // 0=歩行不可, 1=歩行可能
    int width = 0;
    int height = 0;
    DirectX::XMFLOAT3 origin{};
    float cellSize = 0.0f;
    float avgHeight = 0.0f;

    // 外周ライン（キャッシュ）
    std::vector<DirectX::XMFLOAT3> boundaryLines; // 2点ずつペア

    bool IsValid() const { return !cells.empty() && width > 0 && height > 0; }
};

// 空間分割用グリッド（高速なポリゴン検索用）
struct PolygonGrid
{
    std::vector<std::vector<uint32_t>> cells; // 各セルに含まれるポリゴンIDリスト
    int width = 0;                            // X方向のセル数
    int height = 0;                           // Z方向のセル数
    float cellSize = 5.0f;                    // セルサイズ（デフォルト5m）
    DirectX::XMFLOAT3 origin{};               // グリッド原点
    
    bool IsValid() const { return !cells.empty() && width > 0 && height > 0; }
    
    // ワールド座標からセルインデックスを取得
    int GetCellIndex(float x, float z) const
    {
        int cx = static_cast<int>((x - origin.x) / cellSize);
        int cz = static_cast<int>((z - origin.z) / cellSize);
        cx = std::max(0, std::min(cx, width - 1));
        cz = std::max(0, std::min(cz, height - 1));
        return cz * width + cx;
    }
};

// NavMeshデータ
struct NavMeshData
{
    std::vector<DirectX::XMFLOAT3> vertices; // 頂点座標
    std::vector<NavMeshPolygon> polygons;    // ポリゴン
    DirectX::BoundingBox bounds{};           // 全体のバウンディングボックス
    NavMeshConfig config{};                  // 生成に使用した設定
    WalkableGridData walkableGrid;           // デバッグ描画用
    PolygonGrid polygonGrid;                 // 空間分割（高速検索用）
    
    bool IsValid() const { return !vertices.empty() && !polygons.empty(); }
    void Clear() { vertices.clear(); polygons.clear(); walkableGrid.cells.clear(); polygonGrid.cells.clear(); }
};

// パス検索結果
struct NavMeshPath
{
    std::vector<DirectX::XMFLOAT3> waypoints; // 経路のウェイポイント
    std::vector<uint32_t> polygonPath;        // 通過するポリゴンID
    bool isPartial = false;                   // 部分的なパス（ゴールに到達できない場合）
    
    bool IsValid() const { return !waypoints.empty(); }
    void Clear() { waypoints.clear(); polygonPath.clear(); isPartial = false; }
};

// ボクセル化用の中間データ
struct HeightSpan
{
    int minY = 0;           // 最小高さ（セル単位）
    int maxY = 0;           // 最大高さ（セル単位）
    uint16_t area = 0;      // 領域ID（0=歩行不可）
    int32_t nextIndex = -1; // 同じXZ位置の次のスパン（-1=なし）
};

struct HeightField
{
    int width = 0;                      // X方向のセル数
    int height = 0;                     // Z方向のセル数
    DirectX::XMFLOAT3 origin{};         // グリッド原点
    float cellSize = 0.0f;              // セルサイズ (XZ)
    float cellHeight = 0.0f;            // セル高さ (Y)
    std::vector<HeightSpan> spanStorage; // スパンのストレージ（所有）
    std::vector<int32_t> spans;         // 各セルの最初のスパンインデックス（-1=なし）
    
    // スパンを追加し、インデックスを返す
    int32_t AddSpan(int minY, int maxY, uint16_t area)
    {
        int32_t index = static_cast<int32_t>(spanStorage.size());
        spanStorage.push_back({minY, maxY, area, -1});
        return index;
    }
    
    // セルにスパンをリンク（高さ順でマージ）
    void LinkSpanToCell(int cellIndex, int32_t spanIndex)
    {
        HeightSpan& newSpan = spanStorage[spanIndex];
        
        if (spans[cellIndex] == -1)
        {
            spans[cellIndex] = spanIndex;
            return;
        }
        
        // 既存スパンとマージを試みる
        int32_t prevIdx = -1;
        int32_t curIdx = spans[cellIndex];
        
        while (curIdx >= 0)
        {
            HeightSpan& curSpan = spanStorage[curIdx];
            
            // スパンが重なるか隣接している場合はマージ
            if (newSpan.minY <= curSpan.maxY + 1 && newSpan.maxY >= curSpan.minY - 1)
            {
                // マージ: 範囲を拡張
                curSpan.minY = std::min(curSpan.minY, newSpan.minY);
                curSpan.maxY = std::max(curSpan.maxY, newSpan.maxY);
                // 歩行可能ならそのまま
                if (newSpan.area > 0) curSpan.area = newSpan.area;
                return; // マージ完了、新スパンは使わない
            }
            
            // 新スパンがこのスパンより下にある場合、ここに挿入
            if (newSpan.maxY < curSpan.minY)
            {
                newSpan.nextIndex = curIdx;
                if (prevIdx >= 0)
                    spanStorage[prevIdx].nextIndex = spanIndex;
                else
                    spans[cellIndex] = spanIndex;
                return;
            }
            
            prevIdx = curIdx;
            curIdx = curSpan.nextIndex;
        }
        
        // リストの末尾に追加
        if (prevIdx >= 0)
            spanStorage[prevIdx].nextIndex = spanIndex;
        else
            spans[cellIndex] = spanIndex;
    }
    
    // スパンを取得（nullptrチェック不要）
    HeightSpan* GetSpan(int32_t index)
    {
        return (index >= 0 && index < static_cast<int32_t>(spanStorage.size())) 
               ? &spanStorage[index] : nullptr;
    }
    
    const HeightSpan* GetSpan(int32_t index) const
    {
        return (index >= 0 && index < static_cast<int32_t>(spanStorage.size())) 
               ? &spanStorage[index] : nullptr;
    }
    
    void Clear()
    {
        spanStorage.clear();
        spans.clear();
        width = height = 0;
    }
};

// 領域データ
struct Region
{
    uint32_t id = 0;
    std::vector<std::pair<int, int>> cells; // (x, z) セル座標
    int minX = 0, maxX = 0;
    int minZ = 0, maxZ = 0;
    float height = 0.0f;   // 平均高さ
};

// 輪郭データ
struct Contour
{
    std::vector<DirectX::XMFLOAT3> vertices;  // 輪郭頂点
    uint32_t regionId = 0;
    float height = 0.0f;
};

} // namespace UnoEngine
