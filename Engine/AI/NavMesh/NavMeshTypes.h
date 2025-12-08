#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <cstdint>
#include <limits>

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

// NavMeshデータ
struct NavMeshData
{
    std::vector<DirectX::XMFLOAT3> vertices; // 頂点座標
    std::vector<NavMeshPolygon> polygons;    // ポリゴン
    DirectX::BoundingBox bounds{};           // 全体のバウンディングボックス
    NavMeshConfig config{};                  // 生成に使用した設定
    WalkableGridData walkableGrid;           // デバッグ描画用
    
    bool IsValid() const { return !vertices.empty() && !polygons.empty(); }
    void Clear() { vertices.clear(); polygons.clear(); walkableGrid.cells.clear(); }
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
    int minY = 0;       // 最小高さ（セル単位）
    int maxY = 0;       // 最大高さ（セル単位）
    uint16_t area = 0;  // 領域ID
    HeightSpan* next = nullptr; // 同じXZ位置の次のスパン
};

struct HeightField
{
    int width = 0;                     // X方向のセル数
    int height = 0;                    // Z方向のセル数
    DirectX::XMFLOAT3 origin{};        // グリッド原点
    float cellSize = 0.0f;             // セルサイズ (XZ)
    float cellHeight = 0.0f;           // セル高さ (Y)
    std::vector<HeightSpan*> spans;    // 各セルのスパンリスト
    
    void Clear();
    ~HeightField() { Clear(); }
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
