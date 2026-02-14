#pragma once
#include <string>

class NavMesh;

struct MinimapBounds {
    float minX = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxZ = 0.0f;
};

class MinimapGenerator {
public:
    static bool Generate(
        NavMesh* navMesh,
        const std::string& pngPath,
        const std::string& boundsPath,
        int texSize = 512
    );

    // Generate circular frame overlay (dark corners + glowing ring)
    // circleRatio: fraction of texture used for the transparent circle opening (0.0-1.0)
    static bool GenerateCircularFrame(
        const std::string& pngPath,
        int texSize = 512,
        float circleRatio = 1.0f
    );

    static MinimapBounds LoadBounds(const std::string& boundsPath);
};
