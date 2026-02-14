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

    static MinimapBounds LoadBounds(const std::string& boundsPath);
};
