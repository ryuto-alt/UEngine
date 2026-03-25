#pragma once

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

namespace UnoEngine::Navigation {

/// Recast NavMesh ビルド設定（assets/config/navmesh_build.json から読み込み可能）
struct NavMeshBuildSettings
{
    // ========== Voxel Settings ==========
    float cellSize = 0.1f;
    float cellHeight = 0.2f;

    // ========== Agent Settings ==========
    float agentRadius = 0.5f;
    float agentHeight = 2.0f;
    float agentMaxClimb = 0.3f;
    float agentMaxSlope = 45.0f;

    // ========== Geometry Processing ==========
    float maxSimplificationError = 1.2f;
    float detailSampleDist = 6.0f;
    float detailSampleMaxError = 1.0f;

    // ========== Region ==========
    int minRegionArea = 8;
    int mergeRegionArea = 20;

    // ========== Poly Mesh ==========
    int maxEdgeLength = 12;
    int maxVertsPerPoly = 6;

    // ========== Tiling ==========
    int maxTiles = 32;
    int tileSize = 32;
    float borderSize = 0.0f;

    // ========== Filtering ==========
    bool useMonotone = true;
    bool filterLowHangingObstacles = true;
    bool filterLedgeSpans = true;
    bool filterWalkableLowHeightSpans = true;

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

    /// JSON設定ファイルからパラメータを読み込み（キーが無い場合はデフォルト値を維持）
    bool LoadFromJson(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        try { file >> j; } catch (...) { return false; }

        if (j.contains("voxel")) {
            auto& v = j["voxel"];
            cellSize   = v.value("cellSize", cellSize);
            cellHeight = v.value("cellHeight", cellHeight);
        }
        if (j.contains("agent")) {
            auto& a = j["agent"];
            agentRadius   = a.value("radius", agentRadius);
            agentHeight   = a.value("height", agentHeight);
            agentMaxClimb = a.value("maxClimb", agentMaxClimb);
            agentMaxSlope = a.value("maxSlope", agentMaxSlope);
        }
        if (j.contains("geometry")) {
            auto& g = j["geometry"];
            maxSimplificationError = g.value("maxSimplificationError", maxSimplificationError);
            detailSampleDist       = g.value("detailSampleDist", detailSampleDist);
            detailSampleMaxError   = g.value("detailSampleMaxError", detailSampleMaxError);
        }
        if (j.contains("region")) {
            auto& r = j["region"];
            minRegionArea   = r.value("minArea", minRegionArea);
            mergeRegionArea = r.value("mergeArea", mergeRegionArea);
        }
        if (j.contains("polyMesh")) {
            auto& p = j["polyMesh"];
            maxEdgeLength  = p.value("maxEdgeLength", maxEdgeLength);
            maxVertsPerPoly = p.value("maxVertsPerPoly", maxVertsPerPoly);
        }
        if (j.contains("tiling")) {
            auto& t = j["tiling"];
            maxTiles   = t.value("maxTiles", maxTiles);
            tileSize   = t.value("tileSize", tileSize);
            borderSize = t.value("borderSize", borderSize);
        }
        if (j.contains("filtering")) {
            auto& f = j["filtering"];
            useMonotone                    = f.value("useMonotone", useMonotone);
            filterLowHangingObstacles      = f.value("filterLowHangingObstacles", filterLowHangingObstacles);
            filterLedgeSpans               = f.value("filterLedgeSpans", filterLedgeSpans);
            filterWalkableLowHeightSpans   = f.value("filterWalkableLowHeightSpans", filterWalkableLowHeightSpans);
        }
        return true;
    }
};

} // namespace UnoEngine::Navigation
