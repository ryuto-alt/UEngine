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

// Lerp helper
uint8_t Lerp8(uint8_t a, uint8_t b, float t) {
    return static_cast<uint8_t>(a + (b - a) * t);
}

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

    // Dark Deception style colors
    constexpr uint8_t fillR = 25, fillG = 18, fillB = 50, fillA = 190;
    constexpr uint8_t edgeR = 75, edgeG = 50, edgeB = 110, edgeA = 220;

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
                     fillR, fillG, fillB, fillA);
    }

    // Border detection: mark filled pixels adjacent to transparent as edges
    // Copy fill state first, then overwrite border pixels
    std::vector<bool> filled(texSize * texSize, false);
    for (int i = 0; i < texSize * texSize; ++i) {
        filled[i] = pixels[i * 4 + 3] > 0;
    }
    const int dx4[] = { -1, 1, 0, 0 };
    const int dy4[] = { 0, 0, -1, 1 };
    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            if (!filled[y * texSize + x]) continue;
            bool isBorder = false;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx4[d];
                int ny = y + dy4[d];
                if (nx < 0 || nx >= texSize || ny < 0 || ny >= texSize || !filled[ny * texSize + nx]) {
                    isBorder = true;
                    break;
                }
            }
            if (isBorder) {
                int idx = (y * texSize + x) * 4;
                pixels[idx + 0] = edgeR;
                pixels[idx + 1] = edgeG;
                pixels[idx + 2] = edgeB;
                pixels[idx + 3] = edgeA;
            }
        }
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

bool MinimapGenerator::GenerateCircularFrame(
    const std::string& pngPath,
    int texSize,
    float circleRatio)
{
    std::vector<uint8_t> pixels(texSize * texSize * 4, 0);

    float cx = texSize * 0.5f;
    float cy = texSize * 0.5f;
    // circleRatio controls how much of the texture is the transparent opening
    float radius = texSize * 0.5f * circleRatio - 4.0f;
    float ringWidth = 2.5f;
    float glowWidth = 12.0f;

    // Frame mask: pure black for clean masking against any background
    constexpr uint8_t maskR = 0, maskG = 0, maskB = 0, maskA = 255;
    constexpr uint8_t ringR = 130, ringG = 70, ringB = 200;
    constexpr uint8_t glowR = 60, glowG = 30, glowB = 100;

    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            int idx = (y * texSize + x) * 4;

            if (dist > radius + glowWidth) {
                // Solid dark mask (corners)
                pixels[idx + 0] = maskR;
                pixels[idx + 1] = maskG;
                pixels[idx + 2] = maskB;
                pixels[idx + 3] = maskA;
            } else if (dist > radius + ringWidth) {
                // Outer glow fade: ring -> dark mask
                float t = (dist - radius - ringWidth) / (glowWidth - ringWidth);
                t = t * t; // ease-in for sharper falloff
                pixels[idx + 0] = Lerp8(glowR, maskR, t);
                pixels[idx + 1] = Lerp8(glowG, maskG, t);
                pixels[idx + 2] = Lerp8(glowB, maskB, t);
                pixels[idx + 3] = Lerp8(200, maskA, t);
            } else if (dist > radius) {
                // Bright ring
                pixels[idx + 0] = ringR;
                pixels[idx + 1] = ringG;
                pixels[idx + 2] = ringB;
                pixels[idx + 3] = 240;
            } else if (dist > radius - 3.0f) {
                // Inner glow (subtle inward bloom)
                float t = (radius - dist) / 3.0f;
                pixels[idx + 0] = Lerp8(ringR, 0, t);
                pixels[idx + 1] = Lerp8(ringG, 0, t);
                pixels[idx + 2] = Lerp8(ringB, 0, t);
                pixels[idx + 3] = static_cast<uint8_t>(120 * (1.0f - t));
            } else {
                // Transparent (map shows through)
                pixels[idx + 0] = 0;
                pixels[idx + 1] = 0;
                pixels[idx + 2] = 0;
                pixels[idx + 3] = 0;
            }
        }
    }

    // Ensure output directory exists
    std::filesystem::path pngDir = std::filesystem::path(pngPath).parent_path();
    if (!pngDir.empty()) {
        std::filesystem::create_directories(pngDir);
    }

    return stbi_write_png(pngPath.c_str(), texSize, texSize, 4, pixels.data(), texSize * 4) != 0;
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
