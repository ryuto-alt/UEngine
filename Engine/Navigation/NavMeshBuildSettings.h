#pragma once

namespace UnoEngine::Navigation {

/// Recast NavMesh ビルド設定
struct NavMeshBuildSettings
{
    // ========== Voxel Settings ==========
    float cellSize = 0.1f;          // グリッドサイズ（m）
    float cellHeight = 0.2f;        // 高さ解像度（m）
    
    // ========== Agent Settings ==========
    float agentRadius = 0.5f;       // エージェント半径（m）
    float agentHeight = 2.0f;       // エージェント高さ（m）
    float agentMaxClimb = 0.3f;     // 登れる最大段差（m）
    float agentMaxSlope = 45.0f;    // 登れる最大斜度（度）
    
    // ========== Geometry Processing ==========
    float maxSimplificationError = 1.2f;   // 輪郭単純化誤差
    float detailSampleDist = 6.0f;         // ディテール再分割距離
    float detailSampleMaxError = 1.0f;     // ディテール誤差許容
    
    // ========== Region ==========
    int minRegionArea = 8;          // 最小領域サイズ（セル数）
    int mergeRegionArea = 20;       // マージ対象の最大領域サイズ
    
    // ========== Poly Mesh ==========
    int maxEdgeLength = 12;         // 最大エッジ長（セル数）
    int maxVertsPerPoly = 6;        // ポリゴンあたり最大頂点数
    
    // ========== Tiling ==========
    int maxTiles = 32;              // 最大タイル数
    int tileSize = 32;              // タイル解像度（セル数）
    float borderSize = 0.0f;        // タイル境界バッファ（auto）
    
    // ========== Filtering ==========
    bool useMonotone = true;                   // 単調分割（安定性↑）
    bool filterLowHangingObstacles = true;     // 低い障害物をフィルタ
    bool filterLedgeSpans = true;              // 縁のスパンをフィルタ
    bool filterWalkableLowHeightSpans = true;  // 低い歩行可能スパンをフィルタ
    
    // ========== Validation ==========
    [[nodiscard]] bool Validate() const
    {
        return cellSize > 0.0f 
            && cellHeight > 0.0f
            && agentRadius > 0.0f 
            && agentHeight > 0.0f
            && agentMaxSlope > 0.0f 
            && agentMaxSlope < 90.0f;
    }
};

} // namespace UnoEngine::Navigation
