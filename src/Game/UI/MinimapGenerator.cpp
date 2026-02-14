#include "MinimapGenerator.h"
#include "NavMesh/NavMesh.h"
#include "../../externals/tinygltf/stb_image_write.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace {

// Bresenham line drawing on RGBA buffer
void DrawLine(std::vector<uint8_t>& pixels, int w, int h,
              int x0, int y0, int x1, int y1,
              uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            int idx = (y0 * w + x0) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

// Scanline fill a triangle on RGBA buffer
void FillTriangle(std::vector<uint8_t>& pixels, int w, int h,
                  int x0, int y0, int x1, int y1, int x2, int y2,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    // Sort vertices by Y
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }

    int totalHeight = y2 - y0;
    if (totalHeight == 0) return;

    for (int y = y0; y <= y2; ++y) {
        if (y < 0 || y >= h) continue;

        bool secondHalf = (y > y1) || (y1 == y0);
        int segmentHeight = secondHalf ? (y2 - y1) : (y1 - y0);
        if (segmentHeight == 0) continue;

        float alpha = static_cast<float>(y - y0) / totalHeight;
        float beta = secondHalf
            ? static_cast<float>(y - y1) / segmentHeight
            : static_cast<float>(y - y0) / segmentHeight;

        int ax = x0 + static_cast<int>((x2 - x0) * alpha);
        int bx = secondHalf
            ? x1 + static_cast<int>((x2 - x1) * beta)
            : x0 + static_cast<int>((x1 - x0) * beta);

        if (ax > bx) std::swap(ax, bx);

        ax = (std::max)(ax, 0);
        bx = (std::min)(bx, w - 1);

        for (int x = ax; x <= bx; ++x) {
            int idx = (y * w + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
    }
}

} // anonymous namespace

bool MinimapGenerator::Generate(
    NavMesh* navMesh,
    const std::string& pngPath,
    const std::string& boundsPath,
    int texSize)
{
    if (!navMesh || !navMesh->IsValid()) return false;

    auto meshData = navMesh->GetDebugMeshData();
    if (meshData.vertices.empty() || meshData.indices.empty()) return false;

    int vertCount = static_cast<int>(meshData.vertices.size()) / 3;

    // Compute XZ bounding box
    float minX = meshData.vertices[0];
    float maxX = meshData.vertices[0];
    float minZ = meshData.vertices[2];
    float maxZ = meshData.vertices[2];

    for (int i = 0; i < vertCount; ++i) {
        float x = meshData.vertices[i * 3 + 0];
        float z = meshData.vertices[i * 3 + 2];
        minX = (std::min)(minX, x);
        maxX = (std::max)(maxX, x);
        minZ = (std::min)(minZ, z);
        maxZ = (std::max)(maxZ, z);
    }

    // Add 5% margin
    float rangeX = maxX - minX;
    float rangeZ = maxZ - minZ;
    float margin = (std::max)(rangeX, rangeZ) * 0.05f;
    minX -= margin;
    maxX += margin;
    minZ -= margin;
    maxZ += margin;
    rangeX = maxX - minX;
    rangeZ = maxZ - minZ;

    // Use uniform scale so aspect ratio is preserved
    float maxRange = (std::max)(rangeX, rangeZ);
    float scale = (texSize - 1) / maxRange;
    float offsetX = (maxRange - rangeX) * 0.5f;
    float offsetZ = (maxRange - rangeZ) * 0.5f;

    // Adjust bounds to account for uniform scaling
    minX -= offsetX;
    maxX += offsetX;
    minZ -= offsetZ;
    maxZ += offsetZ;

    // Lambda: world XZ -> pixel (Z is flipped so +Z = top of image)
    auto toPixel = [&](float wx, float wz) -> std::pair<int, int> {
        int px = static_cast<int>((wx - minX) * scale);
        int py = (texSize - 1) - static_cast<int>((wz - minZ) * scale);
        return { px, py };
    };

    // RGBA buffer, all transparent
    std::vector<uint8_t> pixels(texSize * texSize * 4, 0);

    // Rasterize triangles (fill)
    int triCount = static_cast<int>(meshData.indices.size()) / 3;
    for (int t = 0; t < triCount; ++t) {
        int i0 = meshData.indices[t * 3 + 0];
        int i1 = meshData.indices[t * 3 + 1];
        int i2 = meshData.indices[t * 3 + 2];

        auto [px0, py0] = toPixel(meshData.vertices[i0 * 3 + 0], meshData.vertices[i0 * 3 + 2]);
        auto [px1, py1] = toPixel(meshData.vertices[i1 * 3 + 0], meshData.vertices[i1 * 3 + 2]);
        auto [px2, py2] = toPixel(meshData.vertices[i2 * 3 + 0], meshData.vertices[i2 * 3 + 2]);

        FillTriangle(pixels, texSize, texSize, px0, py0, px1, py1, px2, py2,
                     50, 50, 60, 180);
    }

    // Draw edges
    for (int t = 0; t < triCount; ++t) {
        int i0 = meshData.indices[t * 3 + 0];
        int i1 = meshData.indices[t * 3 + 1];
        int i2 = meshData.indices[t * 3 + 2];

        auto [px0, py0] = toPixel(meshData.vertices[i0 * 3 + 0], meshData.vertices[i0 * 3 + 2]);
        auto [px1, py1] = toPixel(meshData.vertices[i1 * 3 + 0], meshData.vertices[i1 * 3 + 2]);
        auto [px2, py2] = toPixel(meshData.vertices[i2 * 3 + 0], meshData.vertices[i2 * 3 + 2]);

        DrawLine(pixels, texSize, texSize, px0, py0, px1, py1, 90, 90, 100, 220);
        DrawLine(pixels, texSize, texSize, px1, py1, px2, py2, 90, 90, 100, 220);
        DrawLine(pixels, texSize, texSize, px2, py2, px0, py0, 90, 90, 100, 220);
    }

    // Ensure output directory exists
    std::filesystem::path pngDir = std::filesystem::path(pngPath).parent_path();
    if (!pngDir.empty()) {
        std::filesystem::create_directories(pngDir);
    }

    // Write PNG
    int result = stbi_write_png(pngPath.c_str(), texSize, texSize, 4, pixels.data(), texSize * 4);
    if (!result) return false;

    // Write bounds file
    std::ofstream ofs(boundsPath);
    if (!ofs.is_open()) return false;
    ofs << "minX=" << minX << "\n";
    ofs << "minZ=" << minZ << "\n";
    ofs << "maxX=" << maxX << "\n";
    ofs << "maxZ=" << maxZ << "\n";
    ofs.close();

    return true;
}

MinimapBounds MinimapGenerator::LoadBounds(const std::string& boundsPath) {
    MinimapBounds bounds{};
    std::ifstream ifs(boundsPath);
    if (!ifs.is_open()) return bounds;

    std::string line;
    while (std::getline(ifs, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        float val = std::stof(line.substr(eq + 1));

        if (key == "minX") bounds.minX = val;
        else if (key == "minZ") bounds.minZ = val;
        else if (key == "maxX") bounds.maxX = val;
        else if (key == "maxZ") bounds.maxZ = val;
    }
    return bounds;
}
